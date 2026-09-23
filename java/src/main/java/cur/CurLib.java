package cur;

import com.sun.jna.*;
import java.nio.charset.Charset;
import java.util.*;

public final class CurLib {
    private CurLib() {}
    private static final Charset GBK = Charset.forName("GBK");

    public static final class Curve {
        public final byte[] deviceGbk, nameGbk;
        public final float[] values;
        private Curve(byte[] d, byte[] n, float[] v) { deviceGbk=d; nameGbk=n; values=v; }
        public String device() { return new String(deviceGbk, GBK).trim(); }
        public String name() { return new String(nameGbk, GBK).trim(); }
    }

    public static final class FileData {
        public final int header0, sampleCount;
        public final List<Curve> curves;
        private FileData(int h, int s, List<Curve> c) { header0=h; sampleCount=s; curves=c; }
        public void write(String path) { NativeOwned n = NativeOwned.from(this); try { check(Native.INSTANCE.cur_write(path, n.file, n.error, n.error.length), n.error); } finally { n.close(); } }
        public void exportSwx(String path) { NativeOwned n = NativeOwned.from(this); try { check(Native.INSTANCE.cur_export_swx(path, n.file, n.error, n.error.length), n.error); } finally { n.close(); } }
    }

    public static FileData read(String path) {
        NativeFile n = new NativeFile(); byte[] error = new byte[512];
        check(Native.INSTANCE.cur_read(path, n, error, error.length), error);
        try {
            n.read(); List<Curve> curves = new ArrayList<>();
            for (int i=0; i<n.curveCount; i++) {
                NativeCurve c = new NativeCurve(n.curves.share((long)i * NativeCurve.SIZE)); c.read();
                float[] values = c.values.getFloatArray(0, n.sampleCount);
                curves.add(new Curve(trim(c.deviceGbk), trim(c.nameGbk), values));
            }
            return new FileData(n.header0, n.sampleCount, curves);
        } finally { Native.INSTANCE.cur_free(n); }
    }

    public static void validate(String path) { byte[] error = new byte[512]; check(Native.INSTANCE.cur_validate(path, error, error.length), error); }
    private static void check(int result, byte[] error) { if (result == 0) throw new IllegalStateException(errorText(error)); }
    private static String errorText(byte[] b) { int n=0; while(n<b.length && b[n]!=0) n++; return new String(b, 0, n, java.nio.charset.StandardCharsets.UTF_8); }
    private static byte[] trim(byte[] b) { int n=0; while(n<b.length && b[n]!=0) n++; while(n>0 && b[n-1]==' ') n--; return Arrays.copyOf(b,n); }

    private static final class NativeCurve extends Structure {
        static final int SIZE = new NativeCurve().size();
        public byte[] deviceGbk = new byte[81], nameGbk = new byte[21];
        public Pointer values;
        NativeCurve() { super(); }
        NativeCurve(Pointer p) { super(p); read(); }
        protected List<String> getFieldOrder() { return Arrays.asList("deviceGbk", "nameGbk", "values"); }
    }
    private static final class NativeFile extends Structure {
        public int header0, curveCount, sampleCount; public Pointer curves;
        protected List<String> getFieldOrder() { return Arrays.asList("header0", "curveCount", "sampleCount", "curves"); }
    }

    private static final class NativeOwned implements AutoCloseable {
        final NativeFile file = new NativeFile(); final byte[] error = new byte[512];
        Memory curveMemory; final List<Memory> values = new ArrayList<>();
        private NativeOwned(FileData source) {
            if (source.curves.isEmpty()) throw new IllegalArgumentException("至少需要时间列");
            if (source.curves.size() > 10000 || source.sampleCount <= 0 || source.sampleCount > 100000000) throw new IllegalArgumentException("CUR 数据规模超出范围");
            try {
                curveMemory = new Memory((long)NativeCurve.SIZE * source.curves.size());
                for (int i=0; i<source.curves.size(); i++) {
                    Curve c=source.curves.get(i); if(c.values.length!=source.sampleCount) throw new IllegalArgumentException("曲线长度不一致");
                    if(c.deviceGbk.length>80||c.nameGbk.length>20) throw new IllegalArgumentException("曲线名称过长");
                    NativeCurve nc=new NativeCurve(); System.arraycopy(c.deviceGbk,0,nc.deviceGbk,0,c.deviceGbk.length); System.arraycopy(c.nameGbk,0,nc.nameGbk,0,c.nameGbk.length);
                    Memory m=new Memory((long)Float.BYTES*source.sampleCount); m.write(0,c.values,0,c.values.length); values.add(m); nc.values=m;
                    nc.write(); nc.getPointer().write(0,nc.getPointer().getByteArray(0,NativeCurve.SIZE),0,NativeCurve.SIZE);
                    Pointer target=curveMemory.share((long)i*NativeCurve.SIZE); target.write(0,nc.getPointer().getByteArray(0,NativeCurve.SIZE),0,NativeCurve.SIZE);
                }
            } catch (RuntimeException | Error e) {
                close();
                throw e;
            }
            file.header0=source.header0; file.curveCount=source.curves.size(); file.sampleCount=source.sampleCount; file.curves=curveMemory; file.write();
        }
        static NativeOwned from(FileData f){return new NativeOwned(f);}
        public void close(){if (curveMemory != null) curveMemory.close(); for(Memory m:values)m.close();}
    }

    private interface Native extends Library {
        Native INSTANCE = Native.load("curlib", Native.class);
        int cur_read(String path, NativeFile out, byte[] error, long size);
        int cur_write(String path, NativeFile file, byte[] error, long size);
        int cur_validate(String path, byte[] error, long size);
        int cur_export_swx(String path, NativeFile file, byte[] error, long size);
        void cur_free(NativeFile file);
    }
}
