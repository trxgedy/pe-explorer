#ifndef PARSER_HPP
#define PARSER_HPP

#include <stdafx.hpp>

namespace parser
{
	class c_parser
	{
	private:
		HANDLE file_handle = nullptr;

		struct exported_function
		{
			std::string function_name{};
			std::uint32_t ordinal;
			std::uint32_t function_rva;
			std::uint32_t function_offset;
			std::uint32_t function_name_offset;
		};

		struct imported_function
		{
			bool is_ordinal;
			std::string name{};
			std::uint16_t hint;
			std::uint32_t ordinal;
			std::uint32_t thunk_rva;
		};

		struct imported_dll
		{
			std::string name{};
			std::uint32_t offset;
			std::uint32_t function_count;
			std::uint32_t bound;
			std::uint32_t original_first_thunk;
			std::uint32_t first_thunk;
			std::uint32_t name_rva;
			std::vector<imported_function> functions;
		};

		auto parse_imports( ) -> void;
		auto parse_exports( ) -> void;

	public:
		c_parser( ) = default;
		~c_parser( )
		{
			if( file_handle != INVALID_HANDLE_VALUE && file_handle != nullptr )
				CloseHandle( file_handle );
		}

		std::string file_path;
		std::uint32_t file_size;
		std::unique_ptr<std::uint8_t[ ]> file_data;

		PIMAGE_DOS_HEADER dos_header;
		PIMAGE_NT_HEADERS nt_headers;
		PIMAGE_EXPORT_DIRECTORY export_directory;

		std::vector<imported_dll> imports;
		std::vector<exported_function> exports;
		std::vector<PIMAGE_SECTION_HEADER> sections;

		auto reset( ) -> void; // handle, data, buffer cleanup
		auto rva_to_offset( std::uint32_t rva ) -> std::uint32_t;
		auto parse( const char* file_path ) -> void;
	};
}

inline auto _parser = std::make_unique<parser::c_parser>( );

#endif // !PARSER_HPP