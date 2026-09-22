#include "stdafx.hpp"

int __stdcall WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, CHAR* lpCmdLine, int nShowCmd )
{
	const auto _window{ std::make_unique<ui::c_window>( ) };

	if( !_window->render_window( "pe-explorer", { 1600, 900 } ) )
		throw std::runtime_error( "failed to create window" );

	return EXIT_SUCCESS;
}