package cur

/*
#cgo CFLAGS: -I..
#cgo windows LDFLAGS: -L.. -lcurlib
#cgo linux LDFLAGS: -L.. -lcurlib
#include "cur.h"
#include <stdlib.h>
*/
import "C"

import (
    "fmt"
    "unsafe"
)

type Curve struct {
    DeviceGBK []byte
    NameGBK   []byte
    Values    []float32
}

type File struct {
    Header0      uint32
    SampleCount  int
    Curves       []Curve
}

func errorText(buf []C.char) string {
    n := 0
    for n < len(buf) && buf[n] != 0 { n++ }
    out := make([]byte, n)
    for i := range out { out[i] = byte(buf[i]) }
    return string(out)
}

func Read(path string) (*File, error) {
    cpath := C.CString(path); defer C.free(unsafe.Pointer(cpath))
    var native C.CurFile
    errorBuf := make([]C.char, 512)
    if C.cur_read(cpath, &native, &errorBuf[0], C.size_t(len(errorBuf))) == 0 {
        return nil, fmt.Errorf("%s", errorText(errorBuf))
    }
    defer C.cur_free(&native)
    result := &File{Header0: uint32(native.header0), SampleCount: int(native.sample_count)}
    nativeCurves := unsafe.Slice(native.curves, int(native.curve_count))
    for _, nativeCurve := range nativeCurves {
        curve := Curve{
            DeviceGBK: cBytes(nativeCurve.device_gbk[:]),
            NameGBK: cBytes(nativeCurve.name_gbk[:]),
            Values: make([]float32, result.SampleCount),
        }
        for i, value := range unsafe.Slice(nativeCurve.values, result.SampleCount) { curve.Values[i] = float32(value) }
        result.Curves = append(result.Curves, curve)
    }
    return result, nil
}

func cBytes(value []C.char) []byte {
    n := 0
    for n < len(value) && value[n] != 0 { n++ }
    out := make([]byte, n); for i := range out { out[i] = byte(value[i]) }; return out
}

func Validate(path string) error {
    cpath := C.CString(path); defer C.free(unsafe.Pointer(cpath))
    errorBuf := make([]C.char, 512)
    if C.cur_validate(cpath, &errorBuf[0], C.size_t(len(errorBuf))) == 0 { return fmt.Errorf("%s", errorText(errorBuf)) }
    return nil
}

func (f *File) call(path string, export bool) error {
    if f == nil || len(f.Curves) == 0 { return fmt.Errorf("至少需要时间列") }
    if len(f.Curves) > 10000 || f.SampleCount <= 0 || f.SampleCount > 100000000 { return fmt.Errorf("CUR 数据规模超出范围") }
    nativeCurves := make([]C.CurCurve, len(f.Curves)); values := make([][]C.float, len(f.Curves))
    for i, curve := range f.Curves {
        if len(curve.Values) != f.SampleCount { return fmt.Errorf("第 %d 条曲线长度错误", i) }
        if len(curve.DeviceGBK) > 80 || len(curve.NameGBK) > 20 { return fmt.Errorf("第 %d 条曲线名称过长", i) }
        for j, value := range curve.DeviceGBK { nativeCurves[i].device_gbk[j] = C.char(value) }
        for j, value := range curve.NameGBK { nativeCurves[i].name_gbk[j] = C.char(value) }
        values[i] = make([]C.float, len(curve.Values)); for j, v := range curve.Values { values[i][j] = C.float(v) }
        nativeCurves[i].values = (*C.float)(unsafe.Pointer(&values[i][0]))
    }
    native := C.CurFile{header0: C.uint32_t(f.Header0), curve_count: C.uint32_t(len(nativeCurves)), sample_count: C.uint32_t(f.SampleCount), curves: (*C.CurCurve)(unsafe.Pointer(&nativeCurves[0]))}
    cpath := C.CString(path); defer C.free(unsafe.Pointer(cpath))
    errorBuf := make([]C.char, 512); var ok C.int
    if export { ok = C.cur_export_swx(cpath, &native, &errorBuf[0], C.size_t(len(errorBuf))) } else { ok = C.cur_write(cpath, &native, &errorBuf[0], C.size_t(len(errorBuf))) }
    if ok == 0 { return fmt.Errorf("%s", errorText(errorBuf)) }; return nil
}

func (f *File) Write(path string) error { return f.call(path, false) }
func (f *File) ExportSWX(path string) error { return f.call(path, true) }
