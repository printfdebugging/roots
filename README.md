# roots

## Development Setup
- Run `tooling/dependencies.sh` to install dependencies
- See `tooling/vscode` for an example vscode config

## Bulid Steps
```bash
make init
make debug
```

## Debugging
- Open VSCode ('ms-vscode.cmake-tools', 'ms-vscode.cpptools' etc extensions, see [extensions.json](./config/vscode/.vscode/extensions.json))
- Put a breakpoint & Press `<SHIFT-F5>`

![vscode-debugging](assets/vscode-debugging.png)

## Coding Style
- Don't rely on the default values, always assign a default value whenever you can, like in the struct initializer lists, or to local variables.
- Don't prefix the function names unnecessarily, use `openFile` instead of `editorOpenFile`. For subsystems, use minimal prefixes, `fmInit` instead of `fontManagerInit`.


## LLM Policy

> [!NOTE]
> I do not use LLMs for this project, and I will not take patches written by one.
> Here we read the docs and the code, try things, make mistakes, discuss those on
> IRC, fix them and document the whole process in `log.txt`. The goal is
> learning...
