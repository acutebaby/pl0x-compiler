@echo off
REM 扩展版 PL/0 编译程序编译脚本（Windows / MinGW-w64）
gcc -O2 -o pl0x.exe pl0x.c
if errorlevel 1 goto err
echo 编译成功，生成 pl0x.exe
echo 用法： pl0x.exe 源程序.pl0
goto end
:err
echo 编译失败
:end
pause
