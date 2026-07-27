# C + Python Hello World

本项目首先将 C 函数编译为动态链接库，然后使用 Python 的
`ctypes` 模块加载动态链接库并调用其中的函数。

## 文件说明

- `hello.h`：C 函数声明
- `hello.c`：C 函数实现
- `demo.py`：Python 调用代码
- `Makefile`：编译和运行脚本

## 环境要求

- GCC
- GNU Make
- Python 3

## 编译

```bash
make