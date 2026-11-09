# Connect to Ryujinx's GDB stub (Options > Debug > GDB Stub, port 22225), read Scarlet's Wwise registry, detach.
# Run from the repository root once the game shows its title screen:
#   C:\msys64\ucrt64\bin\gdb-multiarch.exe -x tools/gdb/inspect.gdb
set architecture aarch64
set pagination off
set confirm off
target remote localhost:22225
source tools/gdb/inspect.py
detach
quit
