# Connect to Ryujinx's GDB stub (Options > Debug > GDB Stub, port 22225) and arm tools/gdb/probe.py.
# Run from the repository root:  C:\msys64\ucrt64\bin\gdb-multiarch.exe -x tools/gdb/scarlet.gdb
set architecture aarch64
set pagination off
set confirm off
target remote localhost:22225
monitor get info
source tools/gdb/probe.py
continue
