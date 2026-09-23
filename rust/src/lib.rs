use std::{ffi::CString, os::raw::c_char, path::Path};

#[repr(C)]
struct NativeCurve {
    device_gbk: [c_char; 81],
    name_gbk: [c_char; 21],
    values: *mut f32,
}

#[repr(C)]
struct NativeFile {
    header0: u32,
    curve_count: u32,
    sample_count: u32,
    curves: *mut NativeCurve,
}

extern "C" {
    fn cur_read(path: *const c_char, out: *mut NativeFile, error: *mut c_char, size: usize) -> i32;
    fn cur_write(path: *const c_char, file: *const NativeFile, error: *mut c_char, size: usize) -> i32;
    fn cur_validate(path: *const c_char, error: *mut c_char, size: usize) -> i32;
    fn cur_export_swx(path: *const c_char, file: *const NativeFile, error: *mut c_char, size: usize) -> i32;
    fn cur_free(file: *mut NativeFile);
}

#[derive(Debug, Clone)]
pub struct Curve {
    pub device_gbk: Vec<u8>,
    pub name_gbk: Vec<u8>,
    pub values: Vec<f32>,
}

#[derive(Debug, Clone)]
pub struct CurFile {
    pub header0: u32,
    pub sample_count: usize,
    pub curves: Vec<Curve>,
}

fn error_message(buffer: &[c_char]) -> String {
    let bytes: Vec<u8> = buffer.iter().map(|&x| x as u8).take_while(|&x| x != 0).collect();
    String::from_utf8_lossy(&bytes).into_owned()
}

fn c_path(path: impl AsRef<Path>) -> Result<CString, String> {
    CString::new(path.as_ref().to_string_lossy().as_bytes()).map_err(|_| "路径包含 NUL 字节".to_string())
}

impl CurFile {
    pub fn read(path: impl AsRef<Path>) -> Result<Self, String> {
        let path = c_path(path)?;
        let mut native = NativeFile { header0: 0, curve_count: 0, sample_count: 0, curves: std::ptr::null_mut() };
        let mut error = [0 as c_char; 512];
        let ok = unsafe { cur_read(path.as_ptr(), &mut native, error.as_mut_ptr(), error.len()) };
        if ok == 0 { return Err(error_message(&error)); }
        let result = unsafe {
            let curves = std::slice::from_raw_parts(native.curves, native.curve_count as usize).iter().map(|c| Curve {
                device_gbk: bytes_from_c(&c.device_gbk),
                name_gbk: bytes_from_c(&c.name_gbk),
                values: std::slice::from_raw_parts(c.values, native.sample_count as usize).to_vec(),
            }).collect();
            CurFile { header0: native.header0, sample_count: native.sample_count as usize, curves }
        };
        unsafe { cur_free(&mut native); }
        Ok(result)
    }

    pub fn write(&self, path: impl AsRef<Path>) -> Result<(), String> {
        let native = NativeOwned::from_file(self)?;
        call_write(path, &native.native)
    }

    pub fn export_swx(&self, path: impl AsRef<Path>) -> Result<(), String> {
        let native = NativeOwned::from_file(self)?;
        let path = c_path(path)?;
        let mut error = [0 as c_char; 512];
        let ok = unsafe { cur_export_swx(path.as_ptr(), &native.native, error.as_mut_ptr(), error.len()) };
        if ok == 0 { Err(error_message(&error)) } else { Ok(()) }
    }
}

pub fn validate(path: impl AsRef<Path>) -> Result<(), String> {
    let path = c_path(path)?;
    let mut error = [0 as c_char; 512];
    let ok = unsafe { cur_validate(path.as_ptr(), error.as_mut_ptr(), error.len()) };
    if ok == 0 { Err(error_message(&error)) } else { Ok(()) }
}

fn bytes_from_c<const N: usize>(value: &[c_char; N]) -> Vec<u8> {
    value.iter().map(|&x| x as u8).take_while(|&x| x != 0).collect()
}

struct NativeOwned {
    native: NativeFile,
    curves: Vec<NativeCurve>,
    values: Vec<Vec<f32>>,
}

impl NativeOwned {
    fn from_file(file: &CurFile) -> Result<Self, String> {
        if file.curves.is_empty() { return Err("至少需要时间列".into()); }
        if file.curves.len() > 10000 || file.sample_count == 0 || file.sample_count > 100000000 {
            return Err("CUR 数据规模超出范围".into());
        }
        if file.curves.iter().any(|c| c.values.len() != file.sample_count) { return Err("曲线长度不一致".into()); }
        let values: Vec<Vec<f32>> = file.curves.iter().map(|c| c.values.clone()).collect();
        let mut curves = Vec::with_capacity(file.curves.len());
        for (i, curve) in file.curves.iter().enumerate() {
            if curve.device_gbk.len() > 80 || curve.name_gbk.len() > 20 { return Err(format!("第 {i} 条曲线名称过长")); }
            let mut device = [0 as c_char; 81]; let mut name = [0 as c_char; 21];
            for (d, &v) in device.iter_mut().zip(&curve.device_gbk) { *d = v as c_char; }
            for (d, &v) in name.iter_mut().zip(&curve.name_gbk) { *d = v as c_char; }
            curves.push(NativeCurve { device_gbk: device, name_gbk: name, values: std::ptr::null_mut() });
            curves[i].values = values[i].as_ptr() as *mut f32;
        }
        let mut result = Self { native: NativeFile { header0: file.header0, curve_count: curves.len() as u32, sample_count: file.sample_count as u32, curves: std::ptr::null_mut() }, curves, values };
        result.native.curves = result.curves.as_mut_ptr();
        Ok(result)
    }
}

fn call_write(path: impl AsRef<Path>, file: &NativeFile) -> Result<(), String> {
    let path = c_path(path)?; let mut error = [0 as c_char; 512];
    let ok = unsafe { cur_write(path.as_ptr(), file, error.as_mut_ptr(), error.len()) };
    if ok == 0 { Err(error_message(&error)) } else { Ok(()) }
}
