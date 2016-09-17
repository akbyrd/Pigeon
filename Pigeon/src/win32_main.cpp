#include <Windows.h>
#include <Uxtheme.h>
#pragma comment(lib, "UxTheme.lib")
#include <Vssym32.h>
//TODO: Switch to WRL ComPtr
#include <atlbase.h> //CComPtr

#include "shared.hpp"
#include "audio.hpp"
#include "video.hpp"

//#pragma comment(linker,                                  \
//                "\"/manifestdependency:type='win32'      \
//                name='Microsoft.Windows.Common-Controls' \
//                version='6.0.0.0'                        \
//                processorArchitecture='*'                \
//                publicKeyToken='6595b64144ccf1df'        \
//                language='*'\""                          )

// TODO: Move to resources?
static const c16* GUIDSTR_PIGEON = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	b32 success;
	u32 result;
	HRESULT hr;
	MSG msg = {};


	// TODO: Display notification on change (built-in? custom?)
	// TODO: Hotkey to restart
	// TODO: Sound doesn't play on most devices when cycling audio devices
	// TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	// TODO: Custom sound?
	// TODO: Integrate volume ducking?
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	// TODO: Log failures
	// TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	// TODO: Auto-detect headset being turned on/off
	// TODO: Test with mutliple users. Might need use Local\ namespace for the event

	// NOTE: Handles are closed when process terminates.
	// Events are destroyed when the last handle is destroyed.
	HANDLE singleInstanceEvent = CreateEventW(nullptr, false, false, GUIDSTR_PIGEON);
	if (!singleInstanceEvent) return false;

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		success = SetEvent(singleInstanceEvent);
		if (!success) return false;
	
		result = WaitForSingleObject(singleInstanceEvent, 1000);
		if (result == WAIT_TIMEOUT) return false;
		if (result == WAIT_FAILED ) return false;
	}

	hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr)) return false;


	// Window
	HWND hwnd = {};
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0;//CS_DROPSHADOW;
		windowClass.lpfnWndProc   = WndProc;
		windowClass.cbClsExtra    = 0;
		windowClass.cbWndExtra    = 0;
		windowClass.hInstance     = hInstance;
		windowClass.hIcon         = nullptr;
		windowClass.hCursor       = 0; //TODO: Don't affect cursor state
		windowClass.hbrBackground = nullptr;
		windowClass.lpszMenuName  = nullptr;
		windowClass.lpszClassName = L"Pigeon Notification Class";

		ATOM classAtom = RegisterClassW(&windowClass);
		if (classAtom == INVALID_ATOM) goto Cleanup;

		hwnd = CreateWindowExW(
			WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE, //TODO: WS_EX_TRANSPARENT?
			windowClass.lpszClassName,
			L"Pigeon Notification Window",
			WS_POPUP,
			50, 60, //TODO: Get this from the theme?
			200, 60,
			nullptr,
			nullptr,
			hInstance,
			0
		);
		if (hwnd == INVALID_HANDLE_VALUE) goto Cleanup;

		// TODO: UpdateLayeredWindow
		success = SetLayeredWindowAttributes(hwnd, {}, 242, LWA_ALPHA);
		if (!success) goto Cleanup;

		// DEBUG: Show
		success = ShowWindow(hwnd, nCmdShow);
	}


	// Hotkeys
	const int cycleAudioDeviceHotkeyID = 0;
	success = RegisterHotKey(nullptr, cycleAudioDeviceHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F5);
	if (!success) goto Cleanup;

	const int openPlaybackDevicesHotkeyID = 1;
	success = RegisterHotKey(nullptr, openPlaybackDevicesHotkeyID, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_F5);
	if (!success) goto Cleanup;

	const int cycleRefreshRateHotkeyID = 2;
	success = RegisterHotKey(nullptr, cycleRefreshRateHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

	const int openDisplayAdapterSettingsHotkeyID = 3;
	success = RegisterHotKey(nullptr, openDisplayAdapterSettingsHotkeyID, MOD_CONTROL | MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

	// TODO: BELOW_NORMAL_PRIORITY_CLASS?
	success = SetPriorityClass(GetCurrentProcess(), PROCESS_MODE_BACKGROUND_BEGIN);
	if (!success) goto Cleanup;


	// Message pump
	bool quit = false;
	while (!quit)
	{
		//TODO: I'm unsure if this releases on ALL possible messages
		result = MsgWaitForMultipleObjects(1, &singleInstanceEvent, false, INFINITE, QS_ALLINPUT | QS_ALLPOSTMESSAGE);
		if (result == WAIT_OBJECT_0) PostQuitMessage(0);
		if (result == WAIT_FAILED  ) PostQuitMessage(1);

		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			switch (msg.message)
			{
				case WM_HOTKEY:
					switch (msg.wParam)
					{
						case cycleAudioDeviceHotkeyID:
							CycleDefaultAudioDevice();
							break;

						case openPlaybackDevicesHotkeyID:
							OpenAudioPlaybackDevicesWindow();
							break;

						case cycleRefreshRateHotkeyID:
							CycleRefreshRate();
							break;

						case openDisplayAdapterSettingsHotkeyID:
							OpenDisplayAdapterSettingsWindow();
							break;
					}
					break;

				case WM_QUIT:
					quit = true;
					break;

				case WM_PAINT:
				// TODO: Why am I getting timer messages?
				case WM_TIMER:
				// TODO: Disable mouse messages?
				case WM_MOUSEMOVE:
					break;

				default:
					DebugPrint(L"Unexpected message: %d\n", msg.message);
			}
		}
	}

Cleanup:
	CoUninitialize();

	// TODO: These may be unnecessary, but I don't know what guarantees Windows
	// makes about when SetEvent will cause waiting threads to release. If the
	// release happens immediately, the hotkeys need to be unregistered first.
	UnregisterHotKey(nullptr, cycleAudioDeviceHotkeyID);
	UnregisterHotKey(nullptr, openPlaybackDevicesHotkeyID);
	UnregisterHotKey(nullptr, cycleRefreshRateHotkeyID);
	UnregisterHotKey(nullptr, openDisplayAdapterSettingsHotkeyID);
	SetEvent(singleInstanceEvent);

	// Leak all the things!
	// (Windows destroys everything automatically)

	// TODO: This is wrong
	return LOWORD(msg.wParam);
}

inline LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
		case WM_CREATE:
		case WM_DESTROY:
			break;

		case WM_ERASEBKGND:
			// TODO: Who the shit is drawing the background?
			return 1;

		case WM_PAINT:
		{
			// TODO: Handle errors
			i32 result;
			b32 success;


			// Prep
			PAINTSTRUCT paintStruct = {};
			HDC deviceContext = BeginPaint(hwnd, &paintStruct);
			if (deviceContext == nullptr) break;

			// TODO: Ensure string isn't empty
			c16 text[] = L"SteelSeries H Wireless";
			u16 minWidth = 200;
			u16 maxWidth = 300;
			u16 hPadding = 20;


			// Resize
			RECT textSizeRect = {};
			result = DrawTextW(deviceContext, text, -1, &textSizeRect, DT_CALCRECT | DT_SINGLELINE);
			//if (result == 0) break;

			u16 textWidth = textSizeRect.right - textSizeRect.left;

			u16 windowWidth = textWidth + 2*hPadding;
			if (windowWidth < minWidth) windowWidth = minWidth;
			if (windowWidth > maxWidth) windowWidth = maxWidth;

			RECT currentWindowRect = {};
			success = GetWindowRect(hwnd, &currentWindowRect);
			//if (!success) break;

			RECT windowRect = currentWindowRect;
			windowRect.right = windowRect.left + windowWidth;

			if (!EqualRect(&windowRect, &currentWindowRect))
			{
				success = SetWindowPos(
					hwnd,
					nullptr,
					windowRect.left,
					windowRect.top,
					windowRect.right - windowRect.left,
					windowRect.bottom - windowRect.top,
					SWP_DEFERERASE | SWP_NOACTIVATE | SWP_NOREDRAW | SWP_NOREPOSITION | SWP_NOZORDER
				);
				//if (!success) break;
			}


			// Background
			COLORREF backgroundColor = RGB(16, 16, 16);


			// Text
			RECT textRect = windowRect;
			textRect.left   = hPadding;
			textRect.top    = 0;
			textRect.right  = textRect.left + windowWidth - 2*hPadding;
			textRect.bottom = windowRect.bottom - windowRect.top;

			// TODO: Get an appropriate (theme?) font
			result = SetBkMode(deviceContext, TRANSPARENT);
			//if (result == 0) break;

			COLORREF prevColor = {};
			prevColor = SetTextColor(deviceContext, RGB(255, 255, 255));
			//if (prevColor == CLR_INVALID) break;

			//if (SetTextColor(deviceContext, RGB(255, 255, 255))
			//	== CLR_INVALID) break;

			u32 textFormat = DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
			result = DrawTextW(deviceContext, text, -1, &textRect, textFormat);
			//if (result == 0) break;

			EndPaint(hwnd, &paintStruct);

			return 0;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}