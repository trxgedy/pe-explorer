#include "parser.hpp"

namespace parser
{
	auto c_parser::reset( ) -> void
	{
		this->sections.clear( );
		this->imports.clear( );
		this->exports.clear( );
		this->file_path.clear( );
		this->export_directory = nullptr;
		this->dos_header = nullptr;
		this->nt_headers = nullptr;

		if( this->file_handle && this->file_handle != INVALID_HANDLE_VALUE )
		{
			CloseHandle( this->file_handle );
			this->file_handle = nullptr;
		}
	}

	auto c_parser::rva_to_offset( std::uint32_t rva ) -> std::uint32_t
	{
		const auto section_header = IMAGE_FIRST_SECTION( this->nt_headers );

		for( std::uint16_t i = 0; i < this->nt_headers->FileHeader.NumberOfSections; i++ )
		{
			const auto& section = section_header[ i ];

			if( rva >= section.VirtualAddress && rva < section.VirtualAddress + section.Misc.VirtualSize ) // rva E [start_of_section, end_of_section)
				return rva - section.VirtualAddress + section.PointerToRawData;
		}

		return 0;
	}

	auto c_parser::parse_imports( ) -> void
	{
		if( this->nt_headers->Signature != IMAGE_NT_SIGNATURE )
		{
			std::println( "{}, invalid nt headers", __FUNCTION__ );
			return;
		}

		const auto& import_directory = nt_headers->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_IMPORT ];

		if( !import_directory.VirtualAddress )
		{
			std::println( "no imports" );
			return;
		}

		const auto import_descriptor_offset = rva_to_offset( import_directory.VirtualAddress );
		auto import_descriptor = reinterpret_cast< PIMAGE_IMPORT_DESCRIPTOR >( file_data.get( ) + import_descriptor_offset );

		while( import_descriptor->Name )
		{
			imported_dll dll{};

			const auto descriptor_offset = rva_to_offset( import_directory.VirtualAddress ) +
				( reinterpret_cast< std::uint8_t* >( import_descriptor ) - ( file_data.get( ) + rva_to_offset( import_directory.VirtualAddress ) ) );

			const auto import_name_offset = rva_to_offset( import_descriptor->Name );

			dll.offset = descriptor_offset;
			dll.name = reinterpret_cast< const char* >( file_data.get( ) + import_name_offset );
			dll.bound = import_descriptor->TimeDateStamp;
			dll.original_first_thunk = import_descriptor->OriginalFirstThunk;
			dll.first_thunk = import_descriptor->FirstThunk;
			dll.name_rva = import_descriptor->Name;

			const auto thunk_rva = import_descriptor->OriginalFirstThunk != 0
				? import_descriptor->OriginalFirstThunk
				: import_descriptor->FirstThunk;

			const auto thunk_offset = rva_to_offset( thunk_rva );
			auto thunk = reinterpret_cast< PIMAGE_THUNK_DATA >( file_data.get( ) + thunk_offset );

			while( thunk->u1.AddressOfData )
			{
				imported_function func{};

				auto current_thunk_offset = reinterpret_cast< std::uint8_t* >( thunk ) - ( file_data.get( ) + thunk_offset );
				func.thunk_rva = thunk_rva + current_thunk_offset;

				if( IMAGE_SNAP_BY_ORDINAL( thunk->u1.Ordinal ) )
				{
					func.is_ordinal = true;
					func.ordinal = IMAGE_ORDINAL( thunk->u1.Ordinal );
				}
				else
				{
					const auto function_name_offset = rva_to_offset( static_cast< DWORD >( thunk->u1.AddressOfData ) );
					auto function_name = reinterpret_cast< PIMAGE_IMPORT_BY_NAME >( file_data.get( ) + function_name_offset );

					func.is_ordinal = false;
					func.hint = function_name->Hint;
					func.name = function_name->Name;
				}

				dll.functions.emplace_back( func );
				thunk++;
			}

			dll.function_count = static_cast< std::uint32_t >( dll.functions.size( ) );
			this->imports.emplace_back( dll );

			import_descriptor++;
		}
	}

	auto c_parser::parse_exports( ) -> void
	{
		if( this->nt_headers->Signature != IMAGE_NT_SIGNATURE )
		{
			std::println( "{}, invalid nt headers", __FUNCTION__ );
			return;
		}

		const auto& export_directory_entry = nt_headers->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ];

		if( export_directory_entry.VirtualAddress == 0 )
		{
			std::println( "no exports" );
			return;
		}

		const auto export_directory_offset = rva_to_offset( export_directory_entry.VirtualAddress );
		this->export_directory = reinterpret_cast< PIMAGE_EXPORT_DIRECTORY >( file_data.get( ) + export_directory_offset );

		const auto names_offset = rva_to_offset( export_directory->AddressOfNames );
		const auto functions_offset = rva_to_offset( export_directory->AddressOfFunctions );
		const auto ordinals_offset = rva_to_offset( export_directory->AddressOfNameOrdinals );

		const auto names = reinterpret_cast< DWORD* >( file_data.get( ) + names_offset );
		const auto functions = reinterpret_cast< DWORD* >( file_data.get( ) + functions_offset );
		const auto ordinals = reinterpret_cast< WORD* >( file_data.get( ) + ordinals_offset );

		for( auto i = 0; i < export_directory->NumberOfNames; i++ )
		{
			exported_function function;

			function.ordinal = ordinals[ i ] + export_directory->Base;
			function.function_rva = functions[ ordinals[ i ] ];
			function.function_offset = functions_offset + ( ordinals[ i ] * sizeof( DWORD ) );
			function.function_name_offset = rva_to_offset( names[ i ] );
			function.function_name = reinterpret_cast< const char* >( file_data.get( ) + function.function_name_offset );

			this->exports.emplace_back( function );
		}

		std::sort( this->exports.begin( ), this->exports.end( ), []( const exported_function& a, const exported_function& b )
		{
			return a.ordinal < b.ordinal;
		} );
	}

	auto c_parser::parse( const char* file_path ) -> void
	{
		this->reset( );

		this->file_path = file_path;
		this->file_handle = CreateFileA( file_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );

		if( file_handle == INVALID_HANDLE_VALUE )
		{
			MessageBoxA( nullptr, std::format( "invalid handle: {}", GetLastError( ) ).c_str( ), __FUNCTION__, MB_OK );
			return;
		}

		this->file_size = GetFileSize( this->file_handle, nullptr );
		this->file_data = std::make_unique<std::uint8_t[ ]>( file_size );

		if( !ReadFile( this->file_handle, file_data.get( ), file_size, nullptr, nullptr ) )
		{
			MessageBoxA( nullptr, std::format( "read file error: {}", GetLastError( ) ).c_str( ), __FUNCTION__, MB_OK );
			return;
		}

		this->dos_header = reinterpret_cast< PIMAGE_DOS_HEADER >( file_data.get( ) );

		if( dos_header->e_magic != IMAGE_DOS_SIGNATURE )
		{
			this->reset( );
			MessageBoxA( nullptr, "invalid pe file", __FUNCTION__, MB_OK );
			return;
		}

		this->nt_headers = reinterpret_cast< PIMAGE_NT_HEADERS >( file_data.get( ) + dos_header->e_lfanew );

		auto section_array = IMAGE_FIRST_SECTION( this->nt_headers );
		for( std::uint16_t i = 0; i < this->nt_headers->FileHeader.NumberOfSections; i++ )
		{
			sections.emplace_back( &section_array[ i ] );
		}

		this->parse_imports( );
		this->parse_exports( );
	}
}