# CUR 解析库

这是一个纯 C ABI 库，C++、Rust、Python、C# 等语言都可以通过 C ABI 调用。

## 已支持

- 读取 CUR 文件
- 自动识别普通 CUR 和 FDS CUR 文件
- 校验 CUR 文件长度和结构
- 写回 CUR 文件
- 导出为 GBK 字节流的平面 SWX 文本

## 构建

```sh
cmake -S curlib -B build
cmake --build build --config Release
```

Windows 下生成 `curlib.dll` 和 `curlib.lib`，CMake 会自动启用 UTF-8 源文件编译并导出 C ABI；Linux 下生成 `libcurlib.so`。

## C/C++ 调用

```c
#include "cur.h"

char error[256];
CurFile file = {0};
if (!cur_read("IEEE90.CUR", &file, error, sizeof(error))) {
    /* error 是 UTF-8/本地编码的错误信息；曲线名称本身保留 GBK 字节 */
    return 1;
}

float time0 = file.curves[0].values[0];
float angle0 = file.curves[1].values[0];
cur_export_swx("IEEE90-export.SWX", &file, error, sizeof(error));
cur_write("IEEE90-copy.CUR", &file, error, sizeof(error));
cur_free(&file);
```

## 数据布局

`file.curves[0]` 是时间列，`file.curves[1]` 到 `file.curves[27]` 是曲线列；每列有 `file.sample_count` 个 `float`。名称保存在 `device_gbk` 和 `name_gbk` 中，调用方按 GBK 解码。

## SWX 说明

`cur_export_swx` 输出的是单表平面格式，保留原始列顺序和 GBK 曲线名称，便于读取和转换。它不会重建 PSD-BPA 原始 SWX 的多段标题、分组和对齐空格；若需要逐字节兼容原始 SWX，需要另加 SWX 分组模板。

## FDS CUR 文件

`cur_read` 也支持 FDS 生成的 CUR 文件。FDS 文件包含 3 个额外的 100 字节元数据记录，曲线头字段布局也不同；解析器会自动识别该布局，并将末尾缺失的 20 个浮点值补为 `0.0`。

普通 CUR 和 FDS CUR 使用相同的 `cur_read`、`cur_validate`、`cur_write` 和 `cur_export_swx` 接口。FDS 文件读取后导出为 CUR 时会使用标准 CUR 布局，FDS 专用元数据不会写回。

## Python 绑定

绑定位于 `curlib/python/cur.py`，需要先构建动态库：

```python
from curlib.python.cur import read, validate, export_swx

validate("IEEE90.CUR", "build/curlib.dll")
file = read("IEEE90.CUR", "build/curlib.dll")
print(file.curves[1].name, file.curves[1].values[:3])
export_swx("IEEE90-export.SWX", file, "build/curlib.dll")
```

Linux 动态库名称通常为 `libcurlib.so`，Windows 为 `curlib.dll`。

## Rust 绑定

`curlib/rust` 是可直接构建的 Rust crate。它通过 `build.rs` 编译同一份 C 核心代码：

```sh
cargo build --manifest-path curlib/rust/Cargo.toml
```

调用示例：

```rust
use cur_rs::{validate, CurFile};

validate("IEEE90.CUR").unwrap();
let file = CurFile::read("IEEE90.CUR").unwrap();
println!("{}", file.curves[1].values[0]);
file.export_swx("IEEE90-export.SWX").unwrap();
```

Python 和 Rust 都会复制 C 层数据后再释放 C 内存，因此不会产生悬空指针。
