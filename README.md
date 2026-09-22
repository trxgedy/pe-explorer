# PE Explorer

A lightweight Windows PE (Portable Executable) file inspector, built with **ImGui** and **DirectX 11**. Load an `.exe`, `.dll`, or `.sys` file and browse its headers, sections, imports, and exports through a clean, navigable UI.

![Platform](https://img.shields.io/badge/platform-Windows-blue)
![Language](https://img.shields.io/badge/language-C%2B%2B-00599C)
![Renderer](https://img.shields.io/badge/renderer-DirectX%2011-green)

## Features

- **General Info** — file path, size, type (EXE / DLL / SYS), target architecture, subsystem, image base, and entry point at a glance.
- **DOS Header** — raw `IMAGE_DOS_HEADER` fields, including the `MZ` signature and `e_lfanew`.
- **NT Headers** — broken out into three sub-views:
  - **Signature** — the `PE\0\0` signature.
  - **File Header** — machine type, section count, characteristics, and more.
  - **Optional Header** — magic, checksum, subsystem, DLL characteristics, and the rest of `IMAGE_OPTIONAL_HEADER`.
- **Section Headers** — every section (`.text`, `.rdata`, `.data`, etc.) with its virtual/raw addresses, sizes, and characteristics.
- **Imports** — every imported DLL and its imported functions, resolved by name or ordinal.
- **Exports** — every exported function, with ordinal, RVA, and name.

Every field is shown with its **file offset**, so you can cross-reference it directly against a hex editor.

## Preview
https://github.com/user-attachments/assets/a8692ef0-c5fe-4aa1-a097-4bbb6e3567c7

## Building

### Prerequisites

- Windows 10/11
- Visual Studio 2022 (or newer) with the **Desktop development with C++** workload
- Install freetype:x64-windows-static using [vcpkg](https://vcpkg.io/)
- A C++23-capable toolset (the project uses `std::print` and other modern standard library features)

## Project Structure

```
pe-explorer/
├── src/
│   ├── core/
│   │   ├── parser/       # PE parsing logic (headers, sections, imports, exports)
│   │   └── ui/           # ImGui-based interface and DirectX 11 rendering
│   ├── pch/               # Precompiled header
│   └── main.cpp
├── ext/ui/                 # Bundled ImGui + backends
└── notes.md                # Developer notes on the PE format
```

## How It Works

The parser reads the target file's raw bytes into memory (rather than mapping it via `LoadLibrary`), then walks the PE structures manually:

1. Locates `IMAGE_NT_HEADERS` via `IMAGE_DOS_HEADER::e_lfanew`.
2. Reads `IMAGE_SECTION_HEADER` entries to build a map between virtual addresses (RVAs) and on-disk file offsets.
3. Resolves data directory entries (Import Table, Export Table) by converting their RVAs to file offsets using that section map.
4. Walks the import descriptor / thunk arrays and the export directory's parallel arrays to build the final lists shown in the UI.

See [`notes.md`](pe-explorer/notes.md) for a more detailed breakdown of the PE format and the RVA-to-offset conversion used throughout the parser.

## Acknowledgements

- [Dear ImGui](https://github.com/ocornut/imgui) for the UI framework
- Microsoft's [PE Format documentation](https://learn.microsoft.com/en-us/windows/win32/debug/pe-format)
