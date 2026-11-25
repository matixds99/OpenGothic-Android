#!/bin/sh
# builds nblog.dll (32-bit) - no Windows SDK needed
set -e
cd "$(dirname "$0")"
llvm-dlltool -m i386 -k -d kernel32.def -l kernel32.lib
clang -target i686-pc-windows-msvc -O2 -ffreestanding -fno-builtin -fno-stack-protector -fno-exceptions -fno-rtti \
  -nostdlib -x c++ -c nblog.c -o nblog.obj
lld-link /dll /nodefaultlib /entry:DllMain /subsystem:windows /machine:x86 /out:nblog.dll nblog.obj kernel32.lib
