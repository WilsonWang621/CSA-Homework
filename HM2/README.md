# 用 union 解析 IEEE 754 float / double

一个基于 C 语言实现的 IEEE 754 浮点数解析器。

本项目通过 C 语言中的 `union` 访问浮点数在内存中的原始二进制表示，并使用位运算（shift & mask）解析：

- 符号位（Sign）
- 原始指数（Exponent）
- 尾数（Fraction）
- 浮点数类型（Normal / Denormal / Zero / Inf / NaN）

支持：

- IEEE 754 binary32 (`float`)
- IEEE 754 binary64 (`double`)


---

## 编译

```bash
gcc decode.c -o decode
```

## 运行

```bash
./decode
```


---

# 实现原理

## 为什么使用 union？

C 语言中，`float` 和 `double` 类型只能直接以浮点数形式访问。

但是计算机内部实际存储的是 IEEE 754 编码后的二进制数据：

```
浮点数
 |
 v
+----------------+
| IEEE754 bits   |
+----------------+
```

为了直接访问这些 bit，使用 `union`：

```c
union F32 {
    float f;
    uint32_t u;
};
```

`union` 中所有成员共享同一块内存。

例如：

```c
union F32 x;

x.f = 1.0f;
```

此时：

```c
x.f
```

表示：

```
1.0
```

而：

```c
x.u
```

表示：

```
0x3f800000
```

即：

```
0 01111111 00000000000000000000000
```

这就是 `1.0` 的 IEEE 754 二进制表示。


---

# IEEE 754 浮点数格式


## Float (binary32)

`float` 使用 32 bit：

```
31              23              0
+---------------+---------------+
| Sign (1 bit)  | Exp (8 bits)  |
+---------------+---------------+
| Fraction (23 bits)            |
+-------------------------------+
```

组成：

|字段|位数|
|-|-|
|Sign|1 bit|
|Exponent|8 bit|
|Fraction|23 bit|
|Bias|127|


解析：

```c
sign = bits >> 31;

exp = (bits >> 23) & 0xff;

frac = bits & 0x7fffff;
```


---

## Double (binary64)

`double` 使用 64 bit：

```
63          52                    0
+-----------+---------------------+
| Sign      | Exponent            |
+-----------+---------------------+
| Fraction                         |
+----------------------------------+
```

组成：

|字段|位数|
|-|-|
|Sign|1 bit|
|Exponent|11 bit|
|Fraction|52 bit|
|Bias|1023|


解析：

```c
sign = bits >> 63;

exp = (bits >> 52) & 0x7ff;

frac = bits & 0xfffffffffffff;
```


---

# 浮点数分类规则

IEEE 754 根据 Exponent 和 Fraction 判断浮点数类型：

|Exponent|Fraction|类型|
|-|-|-|
|0|0|Zero|
|0|非0|Denormal|
|1 ~ 最大值-1|任意|Normal|
|最大值|0|Infinity|
|最大值|非0|NaN|


---

# 测试样例

程序测试以下六种情况：

```c
0.0
-0.0
1.0
33.0
1.0 / 0.0
0.0 / 0.0
```

这些测试覆盖 IEEE 754 中主要的浮点数类型：

- Zero
- Normal
- Infinity
- NaN


---

# 测试样例解析


## 1. 0.0

IEEE 754 表示：

```
Sign     = 0
Exponent = 0
Fraction = 0
```

判断：

```
Exponent == 0
Fraction == 0
```

因此：

```
Zero
```

输出：

```
sign = 0
exp = 0
frac = 0
zero
```


---

## 2. -0.0

`-0.0` 与 `0.0` 数值相同，但是 IEEE 754 使用符号位区分。

表示：

```
Sign     = 1
Exponent = 0
Fraction = 0
```

由于：

```
Exponent == 0
Fraction == 0
```

仍然属于：

```
Zero
```

输出：

```
sign = 1
exp = 0
frac = 0
zero
```

说明 IEEE 754 可以区分：

```
+0.0
-0.0
```


---

## 3. 1.0

`float` 中：

```
1.0 =
0 01111111 00000000000000000000000
```

拆分：

```
Sign     = 0
Exponent = 127
Fraction = 0
```

因为：

```
0 < Exponent < 255
```

所以属于：

```
Normal
```

输出：

```
sign = 0
exp = 127
frac = 0
normal
```


---

## 4. 33.0

首先：

```
33 = 100001₂
```

正规化：

```
33 = 1.00001 × 2^5
```

float 的指数使用 Bias：

```
Exponent = 5 + 127
         = 132
```

因此：

```
Sign = 0
Exponent = 132
Fraction != 0
```

属于：

```
Normal
```

输出：

```
sign = 0
exp = 132
frac = ...
normal
```


---

## 5. 1.0 / 0.0

IEEE 754 定义：

```
非零数 / 0
```

结果：

```
Infinity
```

float：

```
Exponent = 255
Fraction = 0
```

double：

```
Exponent = 2047
Fraction = 0
```

输出：

```
sign = 0
exp = 255
frac = 0
Inf
```


---

## 6. 0.0 / 0.0

数学上：

```
0 / 0
```

没有确定结果。

IEEE 754 使用：

```
NaN (Not a Number)
```

表示。

判断条件：

```
Exponent = 最大值
Fraction != 0
```

float：

```
Exponent = 255
```

double：

```
Exponent = 2047
```

输出：

```
sign = 0
exp = 255
frac != 0
NaN
```


---

# 示例输出

```
===== FLOAT =====

input: 1.000000

sign = 0
exp = 127
frac = 0
normal


===== DOUBLE =====

input: 33.000000

sign = 0
exp = 1028
frac = ...
normal
```


---

# 项目结构

```
.
├── decode.c
└── README.md
```


---

# 学习目标

通过本项目可以理解：

- IEEE 754 浮点数表示标准
- float / double 的内存布局
- C 语言 union 的使用
- 位运算：
  - 左移 / 右移 (`<<` / `>>`)
  - 按位与 (`&`)
- 二进制数据解析方法


---

# 与计算机系统的联系

本项目核心思想：

```
Binary Data
      |
      v
Bit Extraction
      |
      v
Field Interpretation
      |
      v
Meaning
```

与 CPU 中的指令解析类似：

```
Machine Instruction

Binary
 |
 v
Opcode / Register / Immediate
```

本项目：

```
Floating Point

Binary
 |
 v
Sign / Exponent / Fraction
```

本质都是：

> 从底层二进制数据中提取字段，并解释其含义。


---

# 相关知识

- Computer Architecture
- Computer Systems
- Compiler
- Operating System
- CPU Simulator
- Debugger