#!/bin/bash

MSYS2="clang64/mingw-w64-clang-x86_64"
MSYS2_BASH="/c/msys64/usr/bin/bash"
CLANG64_BIN='C:\msys64\clang64\bin'

WINGET_PACKAGES=(
   MSYS2.MSYS2
   Git.Git
   Microsoft.VisualStudioCode
)

MSYS2_PACKAGES=(
   make
   "${MSYS2}-gdb"
   "${MSYS2}-cmake"
   "${MSYS2}-ninja"
   "${MSYS2}-clang"
   "${MSYS2}-clang-tools-extra"
   "${MSYS2}-ccache"
   "${MSYS2}-git"
   "${MSYS2}-vulkan-headers"
   "${MSYS2}-vulkan-loader"
)

ARCH_PACKAGES=(
   cmake 
   make 
   ninja 
   clang 
   gdb 
   ccache 
   vulkan-devel 
   libasan 
   git
)

BREW_PACKAGES=(
   cmake 
   ninja 
   llvm 
   ccache 
   git 
   vulkan-headers 
   vulkan-loader 
   molten-vk
)

function windowsDeps() {
   winget install --exact --accept-package-agreements --accept-source-agreements "${WINGET_PACKAGES[@]}"
   powershell.exe -NoProfile -Command "if (\$env:Path -notlike '*${CLANG64_BIN}*') { [Environment]::SetEnvironmentVariable('Path', [Environment]::GetEnvironmentVariable('Path', 'User') + ';${CLANG64_BIN}', 'User') }"
   MSYSTEM=CLANG64 "${MSYS2_BASH}" -lc "pacman -Syy --noconfirm ${MSYS2_PACKAGES[*]}"
}

function archlinuxDeps() { sudo pacman -S --needed --noconfirm "${ARCH_PACKAGES[@]}"; }

function brewDeps() {
   if ! command -v brew >/dev/null; then
      /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
      eval "$(/opt/homebrew/bin/brew shellenv 2>/dev/null || /usr/local/bin/brew shellenv)"
   fi
   brew install "${BREW_PACKAGES[@]}"
}

function linuxDeps() {
   local distribution
   distribution="$(. /etc/os-release && echo "${ID}${ID_LIKE:+ ${ID_LIKE}}")"
   case "${distribution}" in
      *arch*) archlinuxDeps ;;
      *)
         echo "no package list for ${distribution}, add one next to archlinuxDeps" >&2
         exit 1
         ;;
   esac
}

case "$(uname -s)" in
   Linux) linuxDeps ;;
   Darwin) brewDeps ;;
   MINGW* | MSYS* | CYGWIN*) windowsDeps ;;
   *)
      echo "unsupported platform: $(uname -s)" >&2
      exit 1
      ;;
esac
