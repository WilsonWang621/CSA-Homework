import ctypes  
import os   

# 获取 demo.py 所在目录，避免受到当前运行目录影响   
current_dir = os.path.dirname(os.path.abspath(__file__))

# 动态链接库路径
library_path = os.path.join(current_dir, "libhello.so")

# 加载动态链接库
hello_library = ctypes.CDLL(library_path)

# 声明 C 函数的返回值类型
hello_library.hello.restype = None

# 调用 C 函数
hello_library.hello()