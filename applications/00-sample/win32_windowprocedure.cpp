
#include "./win32_windowprocedure.hpp"

#include <Windows.h>

namespace blk
{

LRESULT CALLBACK MinimalWindowProcedure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;
		;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
		;
	case WM_KEYUP:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
			;
		default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
		}
	default: return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
}

} // namespace blk
