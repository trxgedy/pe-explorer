#ifndef UI_HPP
#define UI_HPP

#include "stdafx.hpp"

namespace ui
{
	struct context_t
	{
		HWND hwnd{};
		WNDCLASSEX wc{};
		bool context_state = { true };

		ID3D11Device* g_pd3dDevice = nullptr;
		ID3D11DeviceContext* device = nullptr;
		IDXGISwapChain* swap_chain = nullptr;
		ID3D11RenderTargetView* target_view = nullptr;

		bool                     swap_chain_oc = { false };
		UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
	};

	class c_window
	{
	public:
		c_window( ) = default;
		~c_window( ) = default;

		bool render_window( char* name, ImVec2 size );

		inline static ID3D11ShaderResourceView* folder_icon = nullptr, * trashbin_icon = nullptr;

		inline static enum tabs
		{
			TAB_GENERAL,
			TAB_DOS_HEADER,
			TAB_NT_SIGNATURE,
			TAB_FILE_HEADER,
			TAB_OPTIONAL_HEADER,
			TAB_SECTION_HEADERS,
			TAB_IMPORTS,
			TAB_EXPORTS,
		} current_tab = TAB_GENERAL;

	private:
		char* window_name{ };
		ImVec2 w_size;
		context_t w_context{};

		ImFont* consolas = nullptr;

		bool create_device( );
		void cleanup_device( );
		bool create_window( );
		void menu_style( );
		void menu_design( );

	public:
		void on_resize( UINT w, UINT h )
		{
			w_context.g_ResizeWidth = w;
			w_context.g_ResizeHeight = h;
			w_size.x = ( float )w;
			w_size.y = ( float )h;
		}
	};
}

#endif // !UI_HPP