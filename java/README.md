# Java 绑定

Java 绑定使用 JNA 调用 `curlib` 的 C ABI，不需要编写 JNI C++ 胶水代码。

## 构建

```sh
mvn -f curlib/java/pom.xml package
```

运行时需要：

- `curlib.dll`（Windows）或 `libcurlib.so`（Linux）
- JNA 依赖，可由 Maven 自动下载

Windows 下请使用 CMake 构建生成的 DLL。DLL 已导出完整 C ABI，FDS CUR 与普通 CUR 使用相同的 Java API；FDS 文件末尾缺失的 20 个浮点值会由核心库补为 `0.0`。

## 使用

```java
import cur.CurLib;

public class Main {
    public static void main(String[] args) {
        CurLib.validate("IEEE90.CUR");
        CurLib.FileData file = CurLib.read("IEEE90.CUR");

        System.out.println(file.curves.get(1).name());
        System.out.println(file.curves.get(1).values[0]);

        file.exportSwx("IEEE90-export.SWX");
        file.write("IEEE90-copy.CUR");
    }
}
```

`deviceGbk` 和 `nameGbk` 保留原始 GBK 字节，`device()` 和 `name()` 返回解码后的 Java 字符串。
