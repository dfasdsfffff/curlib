using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;

namespace CurLib;

public sealed class Curve
{
    static Curve() => Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

    public byte[] DeviceGbk { get; init; } = Array.Empty<byte>();
    public byte[] NameGbk { get; init; } = Array.Empty<byte>();
    public float[] Values { get; init; } = Array.Empty<float>();
    public string Device => Encoding.GetEncoding(936).GetString(DeviceGbk).TrimEnd(' ', '\0');
    public string Name => Encoding.GetEncoding(936).GetString(NameGbk).TrimEnd(' ', '\0');
}

public sealed class CurFile
{
    public uint Header0 { get; init; }
    public List<Curve> Curves { get; init; } = new();
    public int SampleCount { get; init; }
    public int CurveCount => Curves.Count;

    public static CurFile Read(string path)
    {
        NativeFile native = default;
        byte[] error = new byte[512];
        int ok = Native.cur_read(path, ref native, error, (nuint)error.Length);
        if (ok == 0) throw new InvalidDataException(ErrorText(error));
        try
        {
            var result = new CurFile { Header0 = native.Header0, SampleCount = checked((int)native.SampleCount) };
            for (int i = 0; i < native.CurveCount; i++)
            {
                IntPtr p = IntPtr.Add(native.Curves, i * Marshal.SizeOf<NativeCurve>());
                NativeCurve item = Marshal.PtrToStructure<NativeCurve>(p);
                float[] values = new float[result.SampleCount];
                Marshal.Copy(item.Values, values, 0, values.Length);
                result.Curves.Add(new Curve
                {
                    DeviceGbk = Trim(item.DeviceGbk),
                    NameGbk = Trim(item.NameGbk),
                    Values = values
                });
            }
            return result;
        }
        finally { Native.cur_free(ref native); }
    }

    public void Write(string path)
    {
        using NativeOwned native = NativeOwned.Create(this);
        byte[] error = new byte[512];
        if (Native.cur_write(path, ref native.File, error, (nuint)error.Length) == 0)
            throw new IOException(ErrorText(error));
    }

    public void ExportSwx(string path)
    {
        using NativeOwned native = NativeOwned.Create(this);
        byte[] error = new byte[512];
        if (Native.cur_export_swx(path, ref native.File, error, (nuint)error.Length) == 0)
            throw new IOException(ErrorText(error));
    }

    public static void Validate(string path)
    {
        byte[] error = new byte[512];
        if (Native.cur_validate(path, error, (nuint)error.Length) == 0)
            throw new InvalidDataException(ErrorText(error));
    }

    private static byte[] Trim(byte[] value)
    {
        int n = Array.IndexOf(value, (byte)0);
        if (n < 0) n = value.Length;
        while (n > 0 && value[n - 1] == 0x20) n--;
        byte[] result = new byte[n];
        Array.Copy(value, result, n);
        return result;
    }

    private static string ErrorText(byte[] value)
    {
        int n = Array.IndexOf(value, (byte)0);
        if (n < 0) n = value.Length;
        return Encoding.UTF8.GetString(value, 0, n);
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeCurve
    {
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 81)] public byte[] DeviceGbk;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 21)] public byte[] NameGbk;
        public IntPtr Values;
    }

    [StructLayout(LayoutKind.Sequential)]
    private struct NativeFile
    {
        public uint Header0;
        public uint CurveCount;
        public uint SampleCount;
        public IntPtr Curves;
    }

    private sealed class NativeOwned : IDisposable
    {
        public NativeFile File;
        private readonly IntPtr _curveMemory;
        private readonly List<IntPtr> _valueMemory = new();
        private bool _disposed;

        private NativeOwned(CurFile source)
        {
            if (source.Curves.Count == 0) throw new ArgumentException("至少需要时间列");
            if (source.Curves.Count > 10000 || source.SampleCount <= 0 || source.SampleCount > 100000000) throw new ArgumentException("CUR 数据规模超出范围");
            foreach (Curve curve in source.Curves)
            {
                if (curve.Values.Length != source.SampleCount) throw new ArgumentException("曲线长度不一致");
                if (curve.DeviceGbk.Length > 80 || curve.NameGbk.Length > 20) throw new ArgumentException("曲线名称过长");
            }
            int size = Marshal.SizeOf<NativeCurve>();
            _curveMemory = Marshal.AllocHGlobal(size * source.Curves.Count);
            try
            {
                for (int i = 0; i < source.Curves.Count; i++)
                {
                    Curve sourceCurve = source.Curves[i];
                    byte[] device = new byte[81]; byte[] name = new byte[21];
                    Array.Copy(sourceCurve.DeviceGbk, device, sourceCurve.DeviceGbk.Length);
                    Array.Copy(sourceCurve.NameGbk, name, sourceCurve.NameGbk.Length);
                    IntPtr values = Marshal.AllocHGlobal(sizeof(float) * source.SampleCount);
                    _valueMemory.Add(values);
                    Marshal.Copy(sourceCurve.Values, 0, values, source.SampleCount);
                    Marshal.StructureToPtr(new NativeCurve { DeviceGbk = device, NameGbk = name, Values = values }, IntPtr.Add(_curveMemory, i * size), false);
                }
            }
            catch
            {
                Dispose();
                throw;
            }
            File = new NativeFile { Header0 = source.Header0, CurveCount = (uint)source.Curves.Count, SampleCount = (uint)source.SampleCount, Curves = _curveMemory };
        }

        public static NativeOwned Create(CurFile source) => new(source);
        public void Dispose()
        {
            if (_disposed) return;
            _disposed = true;
            foreach (IntPtr pointer in _valueMemory) Marshal.FreeHGlobal(pointer);
            Marshal.FreeHGlobal(_curveMemory);
            GC.SuppressFinalize(this);
        }
    }

    private static class Native
    {
        [DllImport("curlib", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int cur_read([MarshalAs(UnmanagedType.LPUTF8Str)] string path, ref NativeFile output, byte[] error, nuint errorSize);
        [DllImport("curlib", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int cur_write([MarshalAs(UnmanagedType.LPUTF8Str)] string path, ref NativeFile file, byte[] error, nuint errorSize);
        [DllImport("curlib", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int cur_validate([MarshalAs(UnmanagedType.LPUTF8Str)] string path, byte[] error, nuint errorSize);
        [DllImport("curlib", CallingConvention = CallingConvention.Cdecl)]
        internal static extern int cur_export_swx([MarshalAs(UnmanagedType.LPUTF8Str)] string path, ref NativeFile file, byte[] error, nuint errorSize);
        [DllImport("curlib", CallingConvention = CallingConvention.Cdecl)]
        internal static extern void cur_free(ref NativeFile file);
    }
}
