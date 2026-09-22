#include "stdafx.hpp"
#include "ui.hpp"

#include "ext\ui\resources\fonts\fonts.hpp"
#include "ext\ui\resources\images\images.hpp"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );

#ifndef GET_X_LPARAM
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#endif
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#endif

namespace ui
{
	static c_window* g_current_window = nullptr;

	LRESULT WINAPI wnd_proc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam )
	{
		if( ImGui_ImplWin32_WndProcHandler( hWnd, msg, wParam, lParam ) )
			return true;

		switch( msg )
		{
			case WM_NCCALCSIZE:
				if( wParam == TRUE )
					return 0;
				break;
			case WM_SIZE:
				if( wParam != SIZE_MINIMIZED && g_current_window )
				{
					g_current_window->on_resize( ( UINT )LOWORD( lParam ), ( UINT )HIWORD( lParam ) );
				}
				return 0;
			case WM_NCHITTEST:
			{
				LRESULT hit = ::DefWindowProcA( hWnd, msg, wParam, lParam );
				if( hit == HTCLIENT )
				{
					POINT pt;
					pt.x = GET_X_LPARAM( lParam );
					pt.y = GET_Y_LPARAM( lParam );
					::ScreenToClient( hWnd, &pt );

					RECT rc;
					::GetClientRect( hWnd, &rc );

					const int border_width = 8;
					bool left = pt.x < border_width;
					bool right = pt.x >= rc.right - border_width;
					bool top = pt.y < border_width;
					bool bottom = pt.y >= rc.bottom - border_width;

					if( top && left ) return HTTOPLEFT;
					if( top && right ) return HTTOPRIGHT;
					if( bottom && left ) return HTBOTTOMLEFT;
					if( bottom && right ) return HTBOTTOMRIGHT;
					if( left ) return HTLEFT;
					if( right ) return HTRIGHT;
					if( top ) return HTTOP;
					if( bottom ) return HTBOTTOM;
				}
				return hit;
			}
			case WM_DESTROY:
				::PostQuitMessage( 0 );
				return 0;
		}
		return ::DefWindowProcA( hWnd, msg, wParam, lParam );
	}

	bool c_window::create_device( )
	{
		DXGI_SWAP_CHAIN_DESC sd;
		ZeroMemory( &sd, sizeof( sd ) );
		sd.BufferCount = 2;
		sd.BufferDesc.Width = 0;
		sd.BufferDesc.Height = 0;
		sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sd.BufferDesc.RefreshRate.Numerator = 60;
		sd.BufferDesc.RefreshRate.Denominator = 1;
		sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		sd.OutputWindow = w_context.hwnd;
		sd.SampleDesc.Count = 1;
		sd.SampleDesc.Quality = 0;
		sd.Windowed = TRUE;
		sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

		UINT createDeviceFlags = 0;
		D3D_FEATURE_LEVEL featureLevel;
		const D3D_FEATURE_LEVEL featureLevelArray[ 2 ] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
		HRESULT res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &w_context.swap_chain, &w_context.g_pd3dDevice, &featureLevel, &w_context.device );
		if( res == DXGI_ERROR_UNSUPPORTED )
			res = D3D11CreateDeviceAndSwapChain( nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &w_context.swap_chain, &w_context.g_pd3dDevice, &featureLevel, &w_context.device );
		if( res != S_OK )
			return false;

		ID3D11Texture2D* pBackBuffer;
		w_context.swap_chain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
		w_context.g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &w_context.target_view );
		pBackBuffer->Release( );

		return true;
	}

	void c_window::cleanup_device( )
	{
		if( w_context.target_view )
		{
			w_context.target_view->Release( );
			w_context.target_view = nullptr;
		}

		if( w_context.swap_chain )
		{
			w_context.swap_chain->Release( );
			w_context.swap_chain = nullptr;
		}
		if( w_context.device )
		{
			w_context.device->Release( );
			w_context.device = nullptr;
		}
		if( w_context.g_pd3dDevice )
		{
			w_context.g_pd3dDevice->Release( );
			w_context.g_pd3dDevice = nullptr;
		}
	}

	bool c_window::create_window( )
	{
		g_current_window = this;
		try
		{
			w_context.wc = { sizeof( w_context.wc ), CS_CLASSDC, wnd_proc, 0L, 0L, GetModuleHandleA( nullptr ), nullptr, nullptr, nullptr, nullptr, "window_class", nullptr };
			RegisterClassExA( &w_context.wc );

			w_context.hwnd = CreateWindowA( w_context.wc.lpszClassName, window_name, WS_POPUP | WS_THICKFRAME, 100, 100, w_size.x, w_size.y, nullptr, nullptr, w_context.wc.hInstance, nullptr );

			if( !create_device( ) )
			{
				cleanup_device( );
				UnregisterClassA( w_context.wc.lpszClassName, w_context.wc.hInstance );
				return false;
			}

			const auto init_centered = []( HWND hwnd )
			{
				RECT rc;
				GetWindowRect( hwnd, &rc );

				auto size = std::make_pair( GetSystemMetrics( SM_CXSCREEN ) - rc.right, GetSystemMetrics( SM_CYSCREEN ) - rc.bottom );

				SetWindowPos( hwnd, nullptr, size.first / 2, size.second / 2, 0, 0, SWP_NOZORDER | SWP_NOSIZE );
			};

			init_centered( w_context.hwnd );

			ShowWindow( w_context.hwnd, SW_SHOWDEFAULT );
			UpdateWindow( w_context.hwnd );

			ImGui::CreateContext( );

			ImGui::StyleColorsDark( );

			ImGui_ImplWin32_Init( w_context.hwnd );
			ImGui_ImplDX11_Init( w_context.g_pd3dDevice, w_context.device );
		}
		catch( std::exception& e )
		{
			cleanup_device( );

			MessageBoxA( nullptr, e.what( ), nullptr, MB_OK | MB_ICONERROR );
			return false;
		}

		return true;
	}

	bool c_window::render_window( char* name, ImVec2 size )
	{
		this->window_name = name;
		this->w_size = size;

		if( !create_window( ) )
			return false;

		D3DX11CreateShaderResourceViewFromMemory( w_context.g_pd3dDevice, images::folder_icon_data, sizeof( images::folder_icon_data ), NULL, NULL, &this->folder_icon, NULL );
		D3DX11CreateShaderResourceViewFromMemory( w_context.g_pd3dDevice, images::trashbin_icon_data, sizeof( images::trashbin_icon_data ), NULL, NULL, &this->trashbin_icon, NULL );

		menu_style( );

		bool done = false;
		while( !done )
		{
			MSG msg;
			while( ::PeekMessageA( &msg, nullptr, 0U, 0U, PM_REMOVE ) )
			{
				::TranslateMessage( &msg );
				::DispatchMessageA( &msg );
				if( msg.message == WM_QUIT )
					done = true;
			}

			if( done || !w_context.context_state )
				break;

			if( w_context.swap_chain_oc && w_context.swap_chain->Present( 0, DXGI_PRESENT_TEST ) == DXGI_STATUS_OCCLUDED )
			{
				::Sleep( 10 );
				continue;
			}

			w_context.swap_chain_oc = false;

			if( w_context.g_ResizeWidth != 0 && w_context.g_ResizeHeight != 0 )
			{
				if( w_context.target_view )
				{
					w_context.target_view->Release( );
					w_context.target_view = nullptr;
				}

				w_context.swap_chain->ResizeBuffers( 0, w_context.g_ResizeWidth, w_context.g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0 );
				w_context.g_ResizeWidth = w_context.g_ResizeHeight = 0;

				ID3D11Texture2D* pBackBuffer;
				w_context.swap_chain->GetBuffer( 0, IID_PPV_ARGS( &pBackBuffer ) );
				w_context.g_pd3dDevice->CreateRenderTargetView( pBackBuffer, nullptr, &w_context.target_view );
				pBackBuffer->Release( );
			}

			SetWindowLongA( w_context.hwnd, GWL_EXSTYLE, WS_EX_LAYERED );
			SetLayeredWindowAttributes( w_context.hwnd, RGB( 0, 0, 0 ), 255, LWA_COLORKEY | LWA_ALPHA );

			ImGui_ImplDX11_NewFrame( );
			ImGui_ImplWin32_NewFrame( );
			ImGui::NewFrame( );
			{
				menu_design( );
			}
			ImGui::Render( );

			const float clear_color_with_alpha[ 4 ] = { 0, 0, 0, 0 };
			w_context.device->OMSetRenderTargets( 1, &w_context.target_view, nullptr );
			w_context.device->ClearRenderTargetView( w_context.target_view, clear_color_with_alpha );
			ImGui_ImplDX11_RenderDrawData( ImGui::GetDrawData( ) );

			HRESULT hr = w_context.swap_chain->Present( 1, 0 );
			w_context.swap_chain_oc = ( hr == DXGI_STATUS_OCCLUDED );

			static auto last_frame_time = std::chrono::steady_clock::now( );
			auto current_time = std::chrono::steady_clock::now( );
			auto frame_time = std::chrono::duration_cast< std::chrono::milliseconds >( current_time - last_frame_time ).count( );

			if( frame_time < 16 )
				std::this_thread::sleep_for( std::chrono::milliseconds( 16 - frame_time ) );

			last_frame_time = std::chrono::steady_clock::now( );
		}

		ImGui_ImplDX11_Shutdown( );
		ImGui_ImplWin32_Shutdown( );
		ImGui::DestroyContext( );

		cleanup_device( );
		DestroyWindow( w_context.hwnd );
		UnregisterClassA( w_context.wc.lpszClassName, w_context.wc.hInstance );

		return true;
	}

	void c_window::menu_style( )
	{
		auto& io = ImGui::GetIO( );
		auto& style = ImGui::GetStyle( );

		consolas = io.Fonts->AddFontFromMemoryCompressedBase85TTF( fonts::consolas_compressed.data( ), 14 );

		io.LogFilename = nullptr;
		io.IniFilename = nullptr;

		style.Colors[ ImGuiCol_WindowBg ] = ImColor( 31, 31, 31 );
		style.Colors[ ImGuiCol_ChildBg ] = ImColor( 35, 35, 35 );
		style.Colors[ ImGuiCol_Border ] = ImColor( 46, 46, 46 );

		style.Colors[ ImGuiCol_Button ] = ImColor( 40, 40, 40, 0 );
		style.Colors[ ImGuiCol_ButtonHovered ] = ImColor( 150, 150, 150, 100 );
		style.Colors[ ImGuiCol_ButtonActive ] = ImColor( 150, 150, 150, 100 );

		style.Colors[ ImGuiCol_FrameBg ] = ImColor( 23, 23, 23 );
		style.Colors[ ImGuiCol_FrameBgHovered ] = ImColor( 25, 25, 25 );
		style.Colors[ ImGuiCol_FrameBgActive ] = ImColor( 25, 25, 25 );

		style.Colors[ ImGuiCol_TableBorderStrong ] = ImColor( 0, 0, 0, 0 );
		style.Colors[ ImGuiCol_TableBorderLight ] = ImColor( 90, 90, 90 );

		style.Colors[ ImGuiCol_ScrollbarBg ] = ImColor( 0, 0, 0, 0 );

		style.WindowPadding = { 0, 0 };
		style.WindowRounding = 0;
		style.WindowBorderSize = 0;
		style.ChildRounding = 0;
		style.ChildBorderSize = 1;
		style.FrameRounding = 0;
		style.GrabRounding = 0;
		style.ScrollbarRounding = 0;
	}

	void c_window::menu_design( )
	{
		ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowRounding, 5.f );
		ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );

		constexpr auto flags{ ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar };

		ImGui::SetNextWindowPos( { 0, 0 }, ImGuiCond_::ImGuiCond_Always );
		ImGui::SetNextWindowSize( w_size, ImGuiCond_::ImGuiCond_Always );

		ImGui::Begin( "###main_painel", nullptr, flags );
		{
			auto draw = ImGui::GetWindowDrawList( );

			draw->AddRectFilled( { 0, 0 }, { w_size.x, 30 }, ImColor( 30, 30, 30 ), 4.f, ImDrawFlags_RoundCornersTop );
			ImGui::SetCursorPos( { 10, 8 } );
			ImGui::Text( "PE Explorer" );

			ImGui::SameLine( );
			ImGui::SetCursorPosY( 5 );
			if( ImGui::ImageButton( "##load_file", ( ImTextureID )c_window::folder_icon, { 16,16 } ) )
			{
				OPENFILENAMEA ofn;
				char szFile[ 260 ] = { 0 };
				ZeroMemory( &ofn, sizeof( ofn ) );
				ofn.lStructSize = sizeof( ofn );
				ofn.hwndOwner = w_context.hwnd;
				ofn.lpstrFile = szFile;
				ofn.nMaxFile = sizeof( szFile );
				ofn.lpstrFilter = "Executables\0*.exe;*.dll;*.sys\0All\0*.*\0";
				ofn.nFilterIndex = 1;
				ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

				if( GetOpenFileNameA( &ofn ) == TRUE )
				{
					_parser->parse( ofn.lpstrFile );
				}
			}
			if( ImGui::IsItemHovered( ) )
			{
				ImGui::SetTooltip( "Load File" );
			}

			ImGui::SameLine( );
			ImGui::SetCursorPosY( 5 );

			if( ImGui::ImageButton( "##unload_file", ( ImTextureID )c_window::trashbin_icon, { 16,16 } ) )
				_parser->reset( );

			if( ImGui::IsItemHovered( ) )
				ImGui::SetTooltip( "Unload File" );

			draw->AddLine( { 0, 30 }, { w_size.x, 30 }, ImColor( 46, 46, 46 ) );

			ImGui::SetCursorPos( { 0, 0 } );
			ImGui::InvisibleButton( "##move", { w_size.x - 50, 30 } );
			if( ImGui::IsItemActive( ) )
			{
				RECT rect;
				GetWindowRect( w_context.hwnd, &rect );
				SetWindowPos( w_context.hwnd, nullptr, rect.left + ImGui::GetMouseDragDelta( ).x, rect.top + ImGui::GetMouseDragDelta( ).y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
			}

			ImGui::SetCursorPos( { w_size.x - 60, 0 } );
			if( ImGui::Button( "-", { 30, 30 } ) )
				ShowWindow( w_context.hwnd, SW_MINIMIZE );

			ImGui::SetCursorPos( { w_size.x - 30, 0 } );
			if( ImGui::Button( "X", { 30, 30 } ) )
				w_context.context_state = false;

			ImGui::SetCursorPos( { 5, 35 } );
			ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 10.f, 10.f } );
			ImGui::BeginChild( "##left_bar", { 220, w_size.y - 35 - 30 }, true, ImGuiWindowFlags_AlwaysUseWindowPadding );
			ImGui::PopStyleVar( );
			{
				ImGui::PushStyleColor( ImGuiCol_Header, ImVec4( 0.2f, 0.4f, 0.8f, 1.0f ) );
				ImGui::PushStyleColor( ImGuiCol_HeaderHovered, ImVec4( 0.2f, 0.45f, 0.9f, 1.0f ) );
				ImGui::PushStyleColor( ImGuiCol_HeaderActive, ImVec4( 0.15f, 0.35f, 0.7f, 1.0f ) );

				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "GENERAL" );
				ImGui::Separator( );
				ImGui::Spacing( );

				std::string display_name = "No File Loaded";
				if( !_parser->file_path.empty( ) )
				{
					size_t last_slash = _parser->file_path.find_last_of( "/\\" );
					display_name = ( last_slash == std::string::npos ) ? _parser->file_path : _parser->file_path.substr( last_slash + 1 );
				}

				if( ImGui::Selectable( display_name.c_str( ), current_tab == tabs::TAB_GENERAL ) )
					current_tab = tabs::TAB_GENERAL;

				ImGui::Spacing( );
				ImGui::Spacing( );

				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "HEADERS" );
				ImGui::Separator( );
				ImGui::Spacing( );

				if( ImGui::Selectable( "DOS Header", current_tab == tabs::TAB_DOS_HEADER ) )
					current_tab = tabs::TAB_DOS_HEADER;

				static bool nt_headers_open = true;
				if( ImGui::Selectable( "NT Headers", false ) )
					nt_headers_open = !nt_headers_open;

				ImGui::SameLine( ImGui::GetWindowWidth( ) - 25.f );
				ImGui::Text( nt_headers_open ? "v" : "<" );

				if( nt_headers_open )
				{
					ImGui::Indent( 10.f );
					if( ImGui::Selectable( "Signature", current_tab == tabs::TAB_NT_SIGNATURE ) )
						current_tab = tabs::TAB_NT_SIGNATURE;

					if( ImGui::Selectable( "File Headers", current_tab == tabs::TAB_FILE_HEADER ) )
						current_tab = tabs::TAB_FILE_HEADER;

					if( ImGui::Selectable( "Optional Header", current_tab == tabs::TAB_OPTIONAL_HEADER ) )
						current_tab = tabs::TAB_OPTIONAL_HEADER;

					ImGui::Unindent( 10.f );
				}

				ImGui::Spacing( );
				ImGui::Spacing( );

				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "STRUCTURE" );
				ImGui::Separator( );
				ImGui::Spacing( );

				if( ImGui::Selectable( "Section Headers", current_tab == tabs::TAB_SECTION_HEADERS ) )
					current_tab = tabs::TAB_SECTION_HEADERS;

				ImGui::Spacing( );
				ImGui::Spacing( );

				ImGui::TextColored( ImVec4( 0.6f, 0.6f, 0.6f, 1.0f ), "DIRECTORIES" );
				ImGui::Separator( );
				ImGui::Spacing( );

				if( ImGui::Selectable( "Imports", current_tab == tabs::TAB_IMPORTS ) )
					current_tab = tabs::TAB_IMPORTS;

				if( ImGui::Selectable( "Exports", current_tab == tabs::TAB_EXPORTS ) )
					current_tab = tabs::TAB_EXPORTS;

				ImGui::PopStyleColor( 3 );
			}
			ImGui::EndChild( );

			ImGui::SetCursorPos( { 5, w_size.y - 25 } );
			ImGui::BeginChild( "##debug_footer", { 220, 20 }, false );
			ImGui::TextColored( ImVec4( 0.5f, 0.5f, 0.5f, 1.0f ), "FPS: %d", static_cast< std::uint32_t >( ImGui::GetIO( ).Framerate ) );
			ImGui::EndChild( );

			constexpr auto draw_row = []( auto offset, const char* name, auto val1, auto val2 )
			{
				ImGui::TableNextRow( );
				ImGui::TableSetColumnIndex( 0 );
				ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );
				ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "%s", std::format( "0x{:X}", offset ).c_str( ) );
				ImGui::TableSetColumnIndex( 1 );
				ImGui::Text( "%s", name );
				ImGui::TableSetColumnIndex( 2 );
				ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "%s", std::format( "0x{:X}", val1 ).c_str( ) );
				ImGui::TableSetColumnIndex( 3 );
				if constexpr( std::is_integral_v< std::decay_t< decltype( val2 ) > > )
					ImGui::Text( "%s", std::format( "0x{:X}", val2 ).c_str( ) );
				else
					ImGui::Text( "%s", val2 );
			};

			ImGui::SetCursorPos( { 230, 35 } );
			ImGui::BeginChild( "##right_content", { w_size.x - 235, w_size.y - 40 }, true );
			{
				constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY;

				switch( current_tab )
				{
					case tabs::TAB_GENERAL:
					{
						if( ImGui::BeginTable( "##general_table", 2, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Property", ImGuiTableColumnFlags_WidthFixed, 120.0f );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( !_parser->file_path.empty( ) )
							{
								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::Text( "File Path" );
								ImGui::TableSetColumnIndex( 1 );
								ImGui::Text( "%s", _parser->file_path.c_str( ) );

								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::Text( "File Size" );
								ImGui::TableSetColumnIndex( 1 );
								ImGui::Text( "%u bytes", _parser->file_size );

								if( _parser->nt_headers )
								{
									std::string type = "Unknown";
									auto chars = _parser->nt_headers->FileHeader.Characteristics;
									auto subsys = _parser->nt_headers->OptionalHeader.Subsystem;

									if( chars & IMAGE_FILE_DLL )
										type = "DLL";
									else if( subsys == IMAGE_SUBSYSTEM_NATIVE )
										type = "SYS";
									else if( chars & IMAGE_FILE_EXECUTABLE_IMAGE )
										type = "EXE";

									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::Text( "Type" );
									ImGui::TableSetColumnIndex( 1 );
									ImGui::Text( "%s", type.c_str( ) );

									std::string machine_str = "Unknown";
									switch( _parser->nt_headers->FileHeader.Machine )
									{
										case IMAGE_FILE_MACHINE_I386: machine_str = "x86"; break;
										case IMAGE_FILE_MACHINE_AMD64: machine_str = "x64"; break;
										case IMAGE_FILE_MACHINE_ARM64: machine_str = "ARM64"; break;
										case IMAGE_FILE_MACHINE_ARM: machine_str = "ARM"; break;
										case IMAGE_FILE_MACHINE_IA64: machine_str = "IA64"; break;
									}

									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::Text( "Architecture" );
									ImGui::TableSetColumnIndex( 1 );
									ImGui::Text( "%s", machine_str.c_str( ) );

									std::string subsys_str = "Unknown";
									switch( subsys )
									{
										case IMAGE_SUBSYSTEM_NATIVE: subsys_str = "Native Driver"; break;
										case IMAGE_SUBSYSTEM_WINDOWS_GUI: subsys_str = "Windows GUI"; break;
										case IMAGE_SUBSYSTEM_WINDOWS_CUI: subsys_str = "Windows Console"; break;
										case IMAGE_SUBSYSTEM_EFI_APPLICATION: subsys_str = "EFI Application"; break;
										case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER: subsys_str = "EFI Boot Service"; break;
										case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER: subsys_str = "EFI Runtime Driver"; break;
										case IMAGE_SUBSYSTEM_OS2_CUI: subsys_str = "OS/2 Console"; break;
										case IMAGE_SUBSYSTEM_POSIX_CUI: subsys_str = "POSIX Console"; break;
									}

									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::Text( "Subsystem" );
									ImGui::TableSetColumnIndex( 1 );
									ImGui::Text( "%s", subsys_str.c_str( ) );

									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::Text( "Image Base" );
									ImGui::TableSetColumnIndex( 1 );
									ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%llX", static_cast< std::uint64_t >( _parser->nt_headers->OptionalHeader.ImageBase ) );

									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::Text( "Entry Point" );
									ImGui::TableSetColumnIndex( 1 );
									ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", _parser->nt_headers->OptionalHeader.AddressOfEntryPoint );
								}
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_DOS_HEADER:
					{
						if( !_parser->dos_header )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No DOS header found or file not loaded." );
							break;
						}

						if( ImGui::BeginTable( "##pe_table", 4, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( _parser->dos_header )
							{
								draw_row( offsetof( IMAGE_DOS_HEADER, e_magic ), "e_magic", _parser->dos_header->e_magic, "Magic number" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_cblp ), "e_cblp", _parser->dos_header->e_cblp, "Bytes on last page of file" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_cp ), "e_cp", _parser->dos_header->e_cp, "Pages in file" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_crlc ), "e_crlc", _parser->dos_header->e_crlc, "Relocations" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_cparhdr ), "e_cparhdr", _parser->dos_header->e_cparhdr, "Size of header in paragraphs" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_minalloc ), "e_minalloc", _parser->dos_header->e_minalloc, "Minimum extra paragraphs needed" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_maxalloc ), "e_maxalloc", _parser->dos_header->e_maxalloc, "Maximum extra paragraphs needed" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_ss ), "e_ss", _parser->dos_header->e_ss, "Initial (relative) SS value" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_sp ), "e_sp", _parser->dos_header->e_sp, "Initial SP value" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_csum ), "e_csum", _parser->dos_header->e_csum, "Checksum" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_ip ), "e_ip", _parser->dos_header->e_ip, "Initial IP value" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_cs ), "e_cs", _parser->dos_header->e_cs, "Initial (relative) CS value" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_lfarlc ), "e_lfarlc", _parser->dos_header->e_lfarlc, "File address of relocation table" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_ovno ), "e_ovno", _parser->dos_header->e_ovno, "Overlay number" );
								draw_row( offsetof( IMAGE_DOS_HEADER, e_lfanew ), "e_lfanew", _parser->dos_header->e_lfanew, "Offset to NT Headers" );
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_NT_SIGNATURE:
					{
						if( !_parser->nt_headers || !_parser->dos_header )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No NT signature found." );
							break;
						}

						if( ImGui::BeginTable( "##pe_table", 4, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( _parser->nt_headers && _parser->dos_header )
							{
								auto nt_offset = _parser->dos_header->e_lfanew;

								auto offset = nt_offset + offsetof( IMAGE_NT_HEADERS, Signature );
								auto val1 = _parser->nt_headers->Signature;

								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );
								ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "%s", std::format( "0x{:X}", offset ).c_str( ) );
								ImGui::TableSetColumnIndex( 1 );
								ImGui::Text( "Signature" );
								ImGui::TableSetColumnIndex( 2 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "%s", std::format( "0x{:08X}", val1 ).c_str( ) );
								ImGui::TableSetColumnIndex( 3 );
								ImGui::Text( "%s", R"(PE Signature "PE\0\0")" );
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_FILE_HEADER:
					{
						if( !_parser->nt_headers || !_parser->dos_header )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No File header found." );
							break;
						}

						if( ImGui::BeginTable( "##pe_table", 4, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( _parser->nt_headers && _parser->dos_header )
							{
								auto fh_offset = _parser->dos_header->e_lfanew + offsetof( IMAGE_NT_HEADERS, FileHeader );
								const auto& fh = _parser->nt_headers->FileHeader;
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, Machine ), "Machine", fh.Machine, "Architecture type" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, NumberOfSections ), "Number Of Sections", fh.NumberOfSections, "Number of sections" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, TimeDateStamp ), "Time Date Stamp", fh.TimeDateStamp, "Time and date the file was created" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, PointerToSymbolTable ), "Pointer To Symbol Table", fh.PointerToSymbolTable, "Offset of the symbol table" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, NumberOfSymbols ), "Number Of Symbols", fh.NumberOfSymbols, "Number of entries in the symbol table" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, SizeOfOptionalHeader ), "Size Of Optional Header", fh.SizeOfOptionalHeader, "Size of the optional header" );
								draw_row( fh_offset + offsetof( IMAGE_FILE_HEADER, Characteristics ), "Characteristics", fh.Characteristics, "Attributes of the file" );
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_OPTIONAL_HEADER:
					{
						if( !_parser->nt_headers || !_parser->dos_header )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No Optional header found." );
							break;
						}

						if( ImGui::BeginTable( "##pe_table", 4, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( _parser->nt_headers && _parser->dos_header )
							{
								auto oh_offset = _parser->dos_header->e_lfanew + offsetof( IMAGE_NT_HEADERS, OptionalHeader );
								const auto& oh = _parser->nt_headers->OptionalHeader;
								using opt_header = std::remove_reference_t<decltype( oh )>;

								draw_row( oh_offset + offsetof( opt_header, Magic ), "Magic", oh.Magic, oh.Magic == 0x20B ? "x64" : "x32" );
								draw_row( oh_offset + offsetof( opt_header, MajorLinkerVersion ), "Major Linker Version", oh.MajorLinkerVersion, "Linker major version" );
								draw_row( oh_offset + offsetof( opt_header, MinorLinkerVersion ), "Minor Linker Version", oh.MinorLinkerVersion, "Linker minor version" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfCode ), "Size Of Code", oh.SizeOfCode, "Size of the code section" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfInitializedData ), "Size Of Initialized Data", oh.SizeOfInitializedData, "Size of initialized data section" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfUninitializedData ), "Size Of Uninitialized Data", oh.SizeOfUninitializedData, "Size of uninitialized data section" );
								draw_row( oh_offset + offsetof( opt_header, AddressOfEntryPoint ), "Address Of Entry Point", oh.AddressOfEntryPoint, "Pointer to entry point function" );
								draw_row( oh_offset + offsetof( opt_header, BaseOfCode ), "Base Of Code", oh.BaseOfCode, "Pointer to beginning of code section" );

								draw_row( oh_offset + offsetof( opt_header, ImageBase ), "Image Base", oh.ImageBase, "Preferred address of first byte of image" );
								draw_row( oh_offset + offsetof( opt_header, SectionAlignment ), "Section Alignment", oh.SectionAlignment, "Alignment of sections in memory" );
								draw_row( oh_offset + offsetof( opt_header, FileAlignment ), "File Alignment", oh.FileAlignment, "Alignment of sections in file" );
								draw_row( oh_offset + offsetof( opt_header, MajorOperatingSystemVersion ), "Major Operating System Version", oh.MajorOperatingSystemVersion, "Major OS version required" );
								draw_row( oh_offset + offsetof( opt_header, MinorOperatingSystemVersion ), "Minor Operating System Version", oh.MinorOperatingSystemVersion, "Minor OS version required" );
								draw_row( oh_offset + offsetof( opt_header, MajorImageVersion ), "Major Image Version", oh.MajorImageVersion, "Major version of image" );
								draw_row( oh_offset + offsetof( opt_header, MinorImageVersion ), "Minor Image Version", oh.MinorImageVersion, "Minor version of image" );
								draw_row( oh_offset + offsetof( opt_header, MajorSubsystemVersion ), "Major Subsystem Version", oh.MajorSubsystemVersion, "Major version of subsystem" );
								draw_row( oh_offset + offsetof( opt_header, MinorSubsystemVersion ), "Minor Subsystem Version", oh.MinorSubsystemVersion, "Minor version of subsystem" );
								draw_row( oh_offset + offsetof( opt_header, Win32VersionValue ), "Win32 Version Value", oh.Win32VersionValue, "Reserved, must be zero" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfImage ), "Size Of Image", oh.SizeOfImage, "Size of image in memory" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfHeaders ), "Size Of Headers", oh.SizeOfHeaders, "Combined size of DOS, PE, and section headers" );
								draw_row( oh_offset + offsetof( opt_header, CheckSum ), "CheckSum", oh.CheckSum, "Image file checksum" );
								draw_row( oh_offset + offsetof( opt_header, Subsystem ), "Subsystem", oh.Subsystem, "Subsystem required to run this image" );
								draw_row( oh_offset + offsetof( opt_header, DllCharacteristics ), "Dll Characteristics", oh.DllCharacteristics, "DLL characteristics" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfStackReserve ), "Size Of Stack Reserve", oh.SizeOfStackReserve, "Size of stack to reserve" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfStackCommit ), "Size Of Stack Commit", oh.SizeOfStackCommit, "Size of stack to commit" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfHeapReserve ), "Size Of Heap Reserve", oh.SizeOfHeapReserve, "Size of local heap to reserve" );
								draw_row( oh_offset + offsetof( opt_header, SizeOfHeapCommit ), "Size Of Heap Commit", oh.SizeOfHeapCommit, "Size of local heap to commit" );
								draw_row( oh_offset + offsetof( opt_header, LoaderFlags ), "Loader Flags", oh.LoaderFlags, "Reserved, must be zero" );
								draw_row( oh_offset + offsetof( opt_header, NumberOfRvaAndSizes ), "Number Of Rva And Sizes", oh.NumberOfRvaAndSizes, "Number of data-directory entries" );

								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 1 );
								if( ImGui::TreeNodeEx( "Data Directory", ImGuiTreeNodeFlags_SpanFullWidth ) )
								{
									for( int i = 0; i < IMAGE_NUMBEROF_DIRECTORY_ENTRIES; ++i )
									{
										const char* dir_names[ ] = { "Export Directory", "Import Directory", "Resource Directory", "Exception Directory", "Security Directory", "Base Relocation Table", "Debug Directory", "Architecture Specific Data", "RVA of GlobalPtr", "TLS Directory", "Load Config Directory", "Bound Import Directory in headers", "Import Address Table", "Delay Load Import Descriptors", "COM Runtime descriptor", "Reserved" };
										draw_row( oh_offset + offsetof( opt_header, DataDirectory ) + ( i * sizeof( IMAGE_DATA_DIRECTORY ) ), dir_names[ i ], oh.DataDirectory[ i ].VirtualAddress, oh.DataDirectory[ i ].Size );
									}
									ImGui::TreePop( );
								}
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_SECTION_HEADERS:
					{
						if( _parser->sections.empty( ) )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No sections found." );
							break;
						}

						if( ImGui::BeginTable( "##sections_table", 9, flags ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Raw Addr", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Raw Size", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Virtual Addr", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Virtual Size", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Characteristics", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Ptr to Reloc", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Num Reloc", ImGuiTableColumnFlags_WidthFixed, 80.0f );
							ImGui::TableSetupColumn( "Num LineNum", ImGuiTableColumnFlags_WidthFixed, 80.0f );
							ImGui::TableHeadersRow( );

							for( const auto& section : _parser->sections )
							{
								char name[ 9 ] = { 0 };
								memcpy( name, section->Name, 8 );

								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "%s", name );

								ImGui::TableSetColumnIndex( 1 );
								ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "0x%X", section->PointerToRawData );

								ImGui::TableSetColumnIndex( 2 );
								ImGui::Text( "0x%X", section->SizeOfRawData );

								ImGui::TableSetColumnIndex( 3 );
								ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "0x%X", section->VirtualAddress );

								ImGui::TableSetColumnIndex( 4 );
								ImGui::Text( "0x%X", section->Misc.VirtualSize );

								ImGui::TableSetColumnIndex( 5 );
								ImGui::Text( "0x%X", section->Characteristics );

								ImGui::TableSetColumnIndex( 6 );
								ImGui::Text( "0x%X", section->PointerToRelocations );

								ImGui::TableSetColumnIndex( 7 );
								ImGui::Text( "%d", section->NumberOfRelocations );

								ImGui::TableSetColumnIndex( 8 );
								ImGui::Text( "%d", section->NumberOfLinenumbers );
							}

							ImGui::EndTable( );
						}
						break;
					}
					case tabs::TAB_IMPORTS:
					{
						if( _parser->imports.empty( ) )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No imports found." );
							break;
						}

						static int selected_dll_idx = 0;

						float half_height = ( ImGui::GetContentRegionAvail( ).y - ImGui::GetTextLineHeightWithSpacing( ) - 5.0f ) / 2.0f;

						if( ImGui::BeginTable( "##imports_table", 7, flags, ImVec2( 0, half_height ) ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "DLL Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Count", ImGuiTableColumnFlags_WidthFixed, 60.0f );
							ImGui::TableSetupColumn( "Bound", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "OriginalFirstThunk", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "FirstTunk", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Name RVA", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							int idx = 0;
							for( const auto& dll : _parser->imports )
							{
								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );

								bool is_selected = ( selected_dll_idx == idx );

								ImGui::PushStyleColor( ImGuiCol_Text, ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ) );
								{
									if( ImGui::Selectable( std::format( "0x{:X}", dll.offset ).c_str( ), is_selected, ImGuiSelectableFlags_SpanAllColumns ) )
										selected_dll_idx = idx;
								}
								ImGui::PopStyleColor( );

								ImGui::TableSetColumnIndex( 1 );
								ImGui::Text( "%s", dll.name.c_str( ) );
								ImGui::TableSetColumnIndex( 2 );
								ImGui::Text( "%d", dll.function_count );
								ImGui::TableSetColumnIndex( 3 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), dll.bound ? "true" : "false" );
								ImGui::TableSetColumnIndex( 4 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", dll.original_first_thunk );
								ImGui::TableSetColumnIndex( 5 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", dll.first_thunk );
								ImGui::TableSetColumnIndex( 6 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", dll.name_rva );

								idx++;
							}

							ImGui::EndTable( );
						}

						ImGui::Separator( );
						ImGui::Text( "Imported Functions" );

						if( selected_dll_idx >= 0 && selected_dll_idx < _parser->imports.size( ) )
						{
							const auto& selected_dll = _parser->imports[ selected_dll_idx ];

							if( ImGui::BeginTable( "##funcs_table", 4, flags, ImVec2( 0, half_height ) ) )
							{
								ImGui::TableSetupScrollFreeze( 0, 1 );
								ImGui::TableSetupColumn( "Thunk RVA", ImGuiTableColumnFlags_WidthFixed, 90.0f );
								ImGui::TableSetupColumn( "Hint", ImGuiTableColumnFlags_WidthFixed, 60.0f );
								ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
								ImGui::TableSetupColumn( "Ordinal", ImGuiTableColumnFlags_WidthStretch );
								ImGui::TableHeadersRow( );

								for( const auto& func : selected_dll.functions )
								{
									ImGui::TableNextRow( );
									ImGui::TableSetColumnIndex( 0 );
									ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );
									ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "0x%X", func.thunk_rva );

									ImGui::TableSetColumnIndex( 1 );

									if( !func.is_ordinal )
										ImGui::Text( "%d", func.hint );
									else
										ImGui::Text( "-" );

									ImGui::TableSetColumnIndex( 2 );

									if( !func.is_ordinal )
										ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "%s", func.name.c_str( ) );
									else
										ImGui::Text( "-" );

									ImGui::TableSetColumnIndex( 3 );

									if( func.is_ordinal )
										ImGui::Text( "%d", func.ordinal );
									else
										ImGui::Text( "-" );
								}
								ImGui::EndTable( );
							}
						}

						break;
					}
					case tabs::TAB_EXPORTS:
					{
						if( !_parser->export_directory )
						{
							ImGui::TextColored( ImVec4( 1.0f, 0.4f, 0.4f, 1.0f ), "No exports found." );
							break;
						}

						float half_height = ( ImGui::GetContentRegionAvail( ).y - ImGui::GetTextLineHeightWithSpacing( ) - 5.0f ) / 2.0f;

						if( ImGui::BeginTable( "##export_dir_table", 4, flags, ImVec2( 0, half_height ) ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Value", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Description", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							if( _parser->export_directory && _parser->nt_headers )
							{
								auto export_dir_rva = _parser->nt_headers->OptionalHeader.DataDirectory[ IMAGE_DIRECTORY_ENTRY_EXPORT ].VirtualAddress;
								auto ed_offset = _parser->rva_to_offset( export_dir_rva );

								const auto& ed = *_parser->export_directory;
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, Characteristics ), "Characteristics", ed.Characteristics, "Flags" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, TimeDateStamp ), "Time Date Stamp", ed.TimeDateStamp, "Time and date the export data was created" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, MajorVersion ), "Major Version", ed.MajorVersion, "Major version number" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, MinorVersion ), "Minor Version", ed.MinorVersion, "Minor version number" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, Name ), "Name RVA", ed.Name, "The RVA of the DLL name" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, Base ), "Base", ed.Base, "The starting ordinal number" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, NumberOfFunctions ), "Number Of Functions", ed.NumberOfFunctions, "Number of entries in the export address table" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, NumberOfNames ), "Number Of Names", ed.NumberOfNames, "Number of entries in the name pointer table" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, AddressOfFunctions ), "Address Of Functions", ed.AddressOfFunctions, "The RVA of the export address table" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, AddressOfNames ), "Address Of Names", ed.AddressOfNames, "The RVA of the export name pointer table" );
								draw_row( ed_offset + offsetof( IMAGE_EXPORT_DIRECTORY, AddressOfNameOrdinals ), "Address Of Name Ordinals", ed.AddressOfNameOrdinals, "The RVA of the ordinal table" );
							}

							ImGui::EndTable( );
						}

						ImGui::Separator( );
						ImGui::Text( "Exported Functions" );

						if( ImGui::BeginTable( "##exports_funcs_table", 5, flags, ImVec2( 0, half_height ) ) )
						{
							ImGui::TableSetupScrollFreeze( 0, 1 );
							ImGui::TableSetupColumn( "Offset", ImGuiTableColumnFlags_WidthFixed, 90.0f );
							ImGui::TableSetupColumn( "Ordinal", ImGuiTableColumnFlags_WidthFixed, 60.0f );
							ImGui::TableSetupColumn( "Function RVA", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Name RVA", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableSetupColumn( "Name", ImGuiTableColumnFlags_WidthStretch );
							ImGui::TableHeadersRow( );

							for( const auto& func : _parser->exports )
							{
								ImGui::TableNextRow( );
								ImGui::TableSetColumnIndex( 0 );
								ImGui::SetCursorPosX( ImGui::GetCursorPosX( ) + 5.0f );
								ImGui::TextColored( ImVec4( 0.4f, 0.9f, 0.4f, 1.0f ), "0x%X", func.function_offset );
								ImGui::TableSetColumnIndex( 1 );
								ImGui::Text( "%d", func.ordinal );
								ImGui::TableSetColumnIndex( 2 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", func.function_rva );
								ImGui::TableSetColumnIndex( 3 );
								ImGui::TextColored( ImVec4( 0.4f, 0.7f, 1.0f, 1.0f ), "0x%X", func.function_name_offset );
								ImGui::TableSetColumnIndex( 4 );
								ImGui::Text( "%s", func.function_name.empty( ) ? "-" : func.function_name.c_str( ) );
							}
							ImGui::EndTable( );
						}
						break;
					}
				}
			}
			ImGui::EndChild( );

			ImGui::End( );
			ImGui::PopStyleVar( 3 );
		}
	}
}