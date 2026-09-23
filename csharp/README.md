# C# 绑定

文件：`Cur.cs`，目标框架：`.NET 8`。

## 构建

先构建原生库：

```sh
cmake -S curlib -B build
cmake --build build --config Release
```

再构建 C# 绑定：

```sh
dotnet build curlib/csharp/CurLib.csproj
```

运行时将 `curlib.dll`（Windows）或 `libcurlib.so`（Linux）放到程序目录，或者加入系统动态库搜索路径。

Windows DLL 已导出完整 C ABI，并支持 UTF-8 文件路径。普通 CUR 和 FDS CUR 使用相同的 `CurFile.Read`、`Validate`、`Write` 和 `ExportSwx` 方法；FDS 文件末尾缺失的 20 个浮点值会由核心库补为 `0.0`。

## 使用

```csharp
using CurLib;

CurFile.Validate("IEEE90.CUR");
CurFile file = CurFile.Read("IEEE90.CUR");

Console.WriteLine(file.Curves[1].Name);
Console.WriteLine(file.Curves[1].Values[0]);

file.ExportSwx("IEEE90-export.SWX");
file.Write("IEEE90-copy.CUR");
```

`DeviceGbk` 和 `NameGbk` 保留原始 GBK 字节；`Device` 和 `Name` 提供解码后的字符串。C# 绑定会在复制数据后调用 `cur_free`，不会暴露 C 层悬空指针。
