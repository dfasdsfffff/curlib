"""CUR C ABI 的 Python ctypes 封装。"""
from __future__ import annotations

import ctypes
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence


class _Curve(ctypes.Structure):
    _fields_ = [
        ("device_gbk", ctypes.c_char * 81),
        ("name_gbk", ctypes.c_char * 21),
        ("values", ctypes.POINTER(ctypes.c_float)),
    ]


class _NativeFile(ctypes.Structure):
    _fields_ = [
        ("header0", ctypes.c_uint32),
        ("curve_count", ctypes.c_uint32),
        ("sample_count", ctypes.c_uint32),
        ("curves", ctypes.POINTER(_Curve)),
    ]


@dataclass
class Curve:
    """一条曲线；名称保留为 GBK 字节。"""

    device_gbk: bytes
    name_gbk: bytes
    values: list[float]

    @property
    def device(self) -> str:
        return self.device_gbk.decode("gbk", errors="replace").rstrip(" \0")

    @property
    def name(self) -> str:
        return self.name_gbk.decode("gbk", errors="replace").rstrip(" \0")


@dataclass
class CurFile:
    """CUR 文件的 Python 表示。"""

    header0: int
    curves: list[Curve]
    sample_count: int

    @property
    def curve_count(self) -> int:
        return len(self.curves)


def _library(path: str | Path | None = None) -> ctypes.CDLL:
    if path is None:
        names = ("curlib.dll", "libcurlib.so", "libcurlib.dylib")
        roots = (Path(__file__).parent, Path.cwd())
        candidates = [root / name for root in roots for name in names]
    else:
        candidates = [Path(path)]
    for candidate in candidates:
        if candidate.exists():
            return ctypes.CDLL(str(candidate))
    raise FileNotFoundError("找不到 curlib 动态库，请传入 library_path")


def _configure(lib: ctypes.CDLL) -> None:
    lib.cur_read.argtypes = [ctypes.c_char_p, ctypes.POINTER(_NativeFile), ctypes.c_char_p, ctypes.c_size_t]
    lib.cur_read.restype = ctypes.c_int
    lib.cur_write.argtypes = [ctypes.c_char_p, ctypes.POINTER(_NativeFile), ctypes.c_char_p, ctypes.c_size_t]
    lib.cur_write.restype = ctypes.c_int
    lib.cur_validate.argtypes = [ctypes.c_char_p, ctypes.c_char_p, ctypes.c_size_t]
    lib.cur_validate.restype = ctypes.c_int
    lib.cur_export_swx.argtypes = [ctypes.c_char_p, ctypes.POINTER(_NativeFile), ctypes.c_char_p, ctypes.c_size_t]
    lib.cur_export_swx.restype = ctypes.c_int
    lib.cur_free.argtypes = [ctypes.POINTER(_NativeFile)]
    lib.cur_free.restype = None


def _error(buffer: ctypes.Array[ctypes.c_char]) -> str:
    return bytes(buffer).split(b"\0", 1)[0].decode("utf-8", errors="replace")


def read(path: str | Path, library_path: str | Path | None = None) -> CurFile:
    """读取 CUR 文件并复制为 Python 对象。"""
    lib = _library(library_path)
    _configure(lib)
    native = _NativeFile()
    error = ctypes.create_string_buffer(512)
    if not lib.cur_read(os.fspath(path).encode(), ctypes.byref(native), error, len(error)):
        raise ValueError(_error(error))
    try:
        curves = []
        for i in range(native.curve_count):
            item = native.curves[i]
            curves.append(Curve(bytes(item.device_gbk), bytes(item.name_gbk), list(item.values[:native.sample_count])))
        return CurFile(native.header0, curves, native.sample_count)
    finally:
        lib.cur_free(ctypes.byref(native))


def _native(file: CurFile):
    if len(file.curves) < 1 or len(file.curves) > 10000:
        raise ValueError("至少需要时间列")
    if not 0 < file.sample_count <= 100000000:
        raise ValueError("采样点数超出范围")
    arrays = []
    native_curves = (_Curve * len(file.curves))()
    for i, curve in enumerate(file.curves):
        if len(curve.values) != file.sample_count:
            raise ValueError(f"第 {i} 条曲线长度错误")
        if len(curve.device_gbk) > 80 or len(curve.name_gbk) > 20:
            raise ValueError(f"第 {i} 条曲线名称超过字段长度")
        native_curves[i].device_gbk = curve.device_gbk
        native_curves[i].name_gbk = curve.name_gbk
        values = (ctypes.c_float * file.sample_count)(*curve.values)
        arrays.append(values)
        native_curves[i].values = values
    native = _NativeFile(file.header0, len(file.curves), file.sample_count, native_curves)
    return native, native_curves, arrays


def write(path: str | Path, file: CurFile, library_path: str | Path | None = None) -> None:
    """写回 CUR 文件。"""
    lib = _library(library_path); _configure(lib)
    native, _, _ = _native(file)
    error = ctypes.create_string_buffer(512)
    if not lib.cur_write(os.fspath(path).encode(), ctypes.byref(native), error, len(error)):
        raise OSError(_error(error))


def export_swx(path: str | Path, file: CurFile, library_path: str | Path | None = None) -> None:
    """导出平面 SWX 文本。"""
    lib = _library(library_path); _configure(lib)
    native, _, _ = _native(file)
    error = ctypes.create_string_buffer(512)
    if not lib.cur_export_swx(os.fspath(path).encode(), ctypes.byref(native), error, len(error)):
        raise OSError(_error(error))


def validate(path: str | Path, library_path: str | Path | None = None) -> None:
    """校验 CUR 文件，失败时抛出 ValueError。"""
    lib = _library(library_path); _configure(lib)
    error = ctypes.create_string_buffer(512)
    if not lib.cur_validate(os.fspath(path).encode(), error, len(error)):
        raise ValueError(_error(error))
