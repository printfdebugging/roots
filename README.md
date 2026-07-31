# roots

## Environment Setup
- Windows
  - Install MSYS2 with `winget install MSYS2.MSYS2`
  - Add `C:\msys64\clang64\bin` to PATH
    - You can do that from the *Edit the system environment variables* dialog
    - Or run this command (from the codeblock below) in Powershell
  - Install Git with `winget install Git.Git`
  - Install VSCode with `winget install Microsoft.VisualStudioCode`
```powershell
if ($env:Path -notlike "*C:\msys64\clang64\bin*") { [Environment]::SetEnvironmentVariable("Path", [Environment]::GetEnvironmentVariable("Path", "User") + ";C:\msys64\clang64\bin", "User") }
```

## Dependencies
- On Windows, open the `MSYS2 CLANG64` shell
- On Linux, use "THE BASH"
```bash
# MSYS2
pacman -Sy --noconfirm clang64/mingw-w64-clang-x86_64-{gdb,cmake,ninja,clang,clang-tools-extra,ccache,git,vulkan-headers,vulkan-loader} make
# Arch Linux
sudo pacman -S --noconfirm cmake make clang gdb ccache vulkan-devel libasan
```

## Bulid Steps
```bash
git submodule update --init --recursive
cmake -B build
cmake --build build
./build/editor
```
