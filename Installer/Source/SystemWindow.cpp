// The edge of the installer's window, as the system would draw it. The window is its picture
// and nothing else; Windows 11 would round its corners off and put a thin line of its own
// around it. Kept apart from the rest so that the system's headers and the JUCE headers never
// meet.
//
//   nativeWindowHandle   the window, as ComponentPeer::getNativeHandle() gives it

extern "C" void dalune_plainWindowEdge (void* nativeWindowHandle);

#if defined (_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
 #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <windows.h>
#include <dwmapi.h>

#if defined (_MSC_VER)
 #pragma comment (lib, "dwmapi.lib")
#endif

extern "C" void dalune_plainWindowEdge (void* nativeWindowHandle)
{
    if (nativeWindowHandle == nullptr)
        return;

    const HWND window = static_cast<HWND> (nativeWindowHandle);

    // Both are settings of Windows 11. (An older Windows answers that it does not know them:
    // it has neither the line nor the round corners.)
    const DWORD noLine = 0xFFFFFFFE;        // DWMWA_COLOR_NONE
    DwmSetWindowAttribute (window, 34 /* DWMWA_BORDER_COLOR */, &noLine, sizeof (noLine));

    const DWORD square = 1;                 // DWMWCP_DONOTROUND
    DwmSetWindowAttribute (window, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &square, sizeof (square));
}

#else

extern "C" void dalune_plainWindowEdge (void*)
{
}

#endif
