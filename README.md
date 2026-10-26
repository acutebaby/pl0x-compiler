# 扩展版 PL/0 编译程序（第 5 组）

## 1. 目录说明
- `pl0x.c` / `pl0x.h`：扩展版编译程序源程序（编译 + 解释执行，一遍扫描）
- `pl0.c` / `p10.h`：教材附录 A 的 C 版基线程序（仅修正 `int` 关键字冲突）
- `cases/`：12 个测试用例（t1 ~ t12）
- `run/`：每个用例的完整输出与中间文件
  - `fa.tmp`：目标代码（P-代码）清单
  - `fas.tmp`：名字表清单
  - `fa2.tmp`：运行结果
  - `f1.tmp`：源程序清单与错误信息
  - `stdout.txt`：控制台完整输出
- `shots/`：编译与运行截图（PNG）

## 2. 编译方法
Linux / macOS：
```
make            # 或 gcc -O2 -o pl0x pl0x.c -lm
```
Windows（MinGW-w64）：
```
gcc -o pl0x.exe pl0x.c
```

## 3. 运行方法
```
./pl0x cases/t10.pl0
```
程序在标准输出打印源程序清单、名字表、目标代码与运行结果，
同时在当前目录生成 fa.tmp / fas.tmp / fa2.tmp / f1.tmp 四个中间文件。

## 4. 扩充功能一览
注释（// 与 /* */）、标识符下划线、字符串常量、一维数组、
if-then-else、for-to/downto、repeat-until、复合赋值（+= -= *= /= %=）、
自增自减（++ --）、取余（% 与 mod）、div、逻辑运算（and/or/not/&&/||/!）、
开方 sqrt、print（不换行输出）、中文错误提示与行列定位、运行期检查。
