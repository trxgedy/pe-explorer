# LAYOUT
    ┌─────────────────────────────┐
    │ IMAGE_DOS_HEADER            │
    │   e_magic                   │
    │   ...                       │
    │   e_lfanew  ────────────────┼──┐  offset to NT headers
    ├─────────────────────────────┤  │
    │ DOS STUB                    │  │
    ├─────────────────────────────┤  │
    │ IMAGE_NT_HEADERS  <─────────┼──┘
    │   Signature                 │
    │   FileHeader                │
    │   OptionalHeader            │
    │     DataDirectory[16]       │
    ├─────────────────────────────┤
    │ IMAGE_SECTION_HEADER[0]     │
    │ IMAGE_SECTION_HEADER[...]   │ (.text, .data,.rdata, .idata, .rsrc, ...FileHeader.NumberOfSections)
    ├─────────────────────────────┤ │
    │ section .text (code)        │ │
    ├─────────────────────────────┤ │
    │ section .rdata (imports,    │<┘  DataDirectory entries (RVA)
    │  read-only data, etc)       │
    ├─────────────────────────────┤
    │ section .data               │
    ├─────────────────────────────┤
    │             ...             │
    └─────────────────────────────┘

# PIMAGE_NT_HEADERS
	* Signature -> 0x00004550 | "PE\0\0"

	* FileHeader
		* NumberOfSections     -> how many IMAGE_SECTION_HEADER entries exist
		* SizeOfOptionalHeader -> needed to find where section headers start

	* Optional Header
		* Magic -> 0x10B = PE32, 0x20B = PE32+
		* DataDirectory[] -> IMPORT_DIR, EXPORT_DIR, IAT, RESOURCES, ...

# IMAGE_SECTION_HEADER[]
    location: IMAGE_FIRST_SECTION(nt_headers), right after OptionalHeader
    count: FileHeader.NumberOfSections

	* Name              -> 8-byte ascii, NOT necessarily null-terminated (".text\0\0")
	* VirtualAddress    -> RVA where this section starts, once loaded in memory
	* Misc.VirtualSize  -> size of section IN MEMORY
	* PointerToRawData  -> FILE OFFSET where this section starts ON DISK
	* SizeOfRawData     -> size of section ON DISK

# RVA -> FILE OFFSET
    for each section:
        if rva E [VirtualAddress, VirtualAddress + VirtualSize):
            return (rva - VirtualAddress) + PointerToRawData

# IMPORTS
    DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress (RVA)
            |
            V rva_to_offset()
    IMAGE_IMPORT_DESCRIPTOR[] <- one entry PER IMPORTED DLL, last entry is null
            |
            │
            ├─ .Name (RVA) -> rva_to_offset() -> DLL name string ("KERNEL32.dll")
            │
            └─ .OriginalFirstThunk | INT
               .FirstThunk         | IAT
                    │
                    V rva_to_offset()
            IMAGE_THUNK_DATA[] <- one entry per imported function, last entry is null
                    │
               (union: u1.Ordinal / u1.AddressOfData - same bits, two meanings)
                    │
                    ├─ high bit set -> IMAGE_ORDINAL(u1.Ordinal) = ordinal, no name
                    │
                    └─ high bit clear -> u1.AddressOfData (RVA) -> rva_to_offset() 
                                                     -> IMAGE_IMPORT_BY_NAME{ Hint; Name[] }
# EXPORTS
    DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress (RVA)
            │
            V rva_to_offset()
    IMAGE_EXPORT_DIRECTORY <- single struct
            │
            ├─ .AddressOfFunctions (RVA)    -> rva_to_offset() -> DWORD[]  (RVAs to function code)
            ├─ .AddressOfNames     (RVA)    -> rva_to_offset() -> DWORD[]  (RVAs to name strings)
            └─ .AddressOfNameOrdinals (RVA) -> rva_to_offset() -> WORD[]   (indices into AddressOfFunctions)

# TO-DO
    * Resources
    * Debug Directory