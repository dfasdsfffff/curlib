# Go 绑定

该绑定通过 cgo 调用 `curlib` 的 C ABI。

## 构建要求

先构建原生动态库，并让 Go 链接器能够找到它：

```sh
cmake -S curlib -B build
cmake --build build --config Release
```

Linux 下可设置：

```sh
export CGO_LDFLAGS="-L/path/to/curlib/build"
export LD_LIBRARY_PATH="/path/to/curlib/build:$LD_LIBRARY_PATH"
```

Windows 下使用 MinGW/clang 的 Go 环境，并把 `curlib.dll` 放入程序目录或 PATH。

Windows 动态库已导出 `cur_read`、`cur_write`、`cur_validate`、`cur_export_swx` 和 `cur_free`。路径按 UTF-8 传入，中文路径可以直接使用。

该绑定自动支持普通 CUR 和 FDS CUR，调用方式相同；FDS 文件末尾缺失采样值由 C 核心补为 `0.0`。

## 使用

```go
package main

import (
    "fmt"
    cur "cur"
)

func main() {
    if err := cur.Validate("IEEE90.CUR"); err != nil {
        panic(err)
    }
    file, err := cur.Read("IEEE90.CUR")
    if err != nil { panic(err) }
    fmt.Println(file.Curves[1].Values[0])
    if err := file.ExportSWX("IEEE90-export.SWX"); err != nil { panic(err) }
}
```
