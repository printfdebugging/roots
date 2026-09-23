#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && git rev-parse --show-toplevel)"
SRC="${ROOT}/tooling/vscode"
DEST_WS="${ROOT}/.vscode"

function vscode_platform() {
   case "$(uname -s)" in
   Linux) echo linux ;;
   Darwin) echo apple ;;
   MINGW* | MSYS* | CYGWIN*) echo windows ;;
   esac
}

PLATFORM="$(vscode_platform)"
if [ -z "${PLATFORM}" ]; then
   echo "unsupported platform: $(uname -s)" >&2
   exit 1
fi

function vscode_user_dir() {
   case "${PLATFORM}" in
   linux) echo "${HOME}/.config/Code/User" ;;
   apple) echo "${HOME}/Library/Application Support/Code/User" ;;
   windows) echo "${APPDATA}/Code/User" ;;
   esac
}

function vscode_install_user() {
   local dest
   dest="$(vscode_user_dir)"
   mkdir -p "${dest}"
   cp -f "${SRC}/settings.json" "${dest}/settings.json"
   cp -f "${SRC}/keybindings.json" "${dest}/keybindings.json"
   echo "   user   -> ${dest}"
}

function vscode_install_workspace() {
   mkdir -p "${DEST_WS}"
   cp -f "${SRC}/.vscode/settings.json.${PLATFORM}" "${DEST_WS}/settings.json"
   cp -f "${SRC}/.vscode/extensions.json" "${DEST_WS}/extensions.json"
   echo "workspace -> ${DEST_WS} (${PLATFORM})"
}

vscode_install_user &&
   vscode_install_workspace
