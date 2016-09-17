#include <Windows.h>
//#include <Uxtheme.h>
//#pragma comment(lib, "UxTheme.lib")
//#include <Vssym32.h>
//TODO: Switch to WRL ComPtr
#include <atlbase.h> //CComPtr

#include "shared.hpp"
#include "audio.hpp"
#include "video.hpp"

//#pragma comment(linker,"\"/manifestdependency:type='win32' \
//name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
//processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

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
	if (false)
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0;//CS_DROPSHADOW;
		windowClass.lpfnWndProc   = WndProc;
		windowClass.cbClsExtra    = 0;
		windowClass.cbWndExtra    = 0;
		windowClass.hInstance     = hInstance;
		windowClass.hIcon         = nullptr;
		windowClass.hCursor       = 0; //TODO
		windowClass.hbrBackground = CreateSolidBrush(RGB(16, 16, 16)); //TODO
		windowClass.lpszMenuName  = nullptr;
		windowClass.lpszClassName = L"Pigeon Notification Class";


		//windowClass.hbrBackground = GetSysColorBrush(COLOR_SCROLLBAR); //Gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_BACKGROUND); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_ACTIVECAPTION); //Light blue-gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_INACTIVECAPTION); //Light blue-gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_MENU); //Off white
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOW); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOWFRAME); //Gray 100
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_MENUTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_WINDOWTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_CAPTIONTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_ACTIVEBORDER); //Light gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_INACTIVEBORDER); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_APPWORKSPACE); //Light gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_HIGHLIGHT); //Light blue
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_HIGHLIGHTTEXT); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNFACE); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNSHADOW); //Light gray
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_GRAYTEXT); //Gray 109
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_INACTIVECAPTIONTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_BTNHIGHLIGHT); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_3DDKSHADOW); //Gray 105!
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_3DLIGHT); //White
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_INFOTEXT); //Black
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_INFOBK); //Yellow-ish white
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_HOTLIGHT); //Medium blue
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_GRADIENTACTIVECAPTION); //Bluish white
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_GRADIENTINACTIVECAPTION); //Lighter bluish white
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_MENUHILIGHT); //Light blue
		//windowClass.hbrBackground = GetSysColorBrush(COLOR_MENUBAR); //Very light gray

		ATOM classAtom = RegisterClassW(&windowClass);
		if (classAtom == INVALID_ATOM) goto Cleanup;

		hwnd = CreateWindowExW(
			WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE, //TODO: WS_EX_TRANSPARENT?
			windowClass.lpszClassName,
			L"Pigeon Notification Window",
			WS_POPUP,
			50, 60, //TODO
			200, 60, //TODO
			nullptr,
			nullptr,
			hInstance,
			0
		);
		if (hwnd == INVALID_HANDLE_VALUE) goto Cleanup;

		success = SetLayeredWindowAttributes(hwnd, {}, 242, LWA_ALPHA);
		if (!success) goto Cleanup;

		//DEBUG: Show
		success = ShowWindow(hwnd, nCmdShow);

		//OpenThemeData(hwnd, L"TRAYNOTIFY");
		//HTHEME theme = OpenThemeData(hwnd, L"FLYOUT");
		//if (theme != INVALID_HANDLE_VALUE)
		//{
		//}
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

				default:
					DebugPrint(L"Unexpected message: %d", msg.message);
			}
		}
	}

Cleanup:
	CoUninitialize();

	// TODO: These may be unnecessary, but I don't know what guarantees Windows
	// makes about when SetEvent will cause waiting threads to release. If the
	// release happens immediately, the hotkeys need to be unregistered first.
	UnregisterHotKey(nullptr, cycleRefreshRateHotkeyID);
	UnregisterHotKey(nullptr, cycleAudioDeviceHotkeyID);
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
		case WM_ERASEBKGND:
			break;

		case WM_PAINT:
		{
			RECT rc = {};
			GetClientRect(hwnd, &rc);

			HFONT hFont = CreateFontW(22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, 0, 0, 0, 0, 0, L"Segoe UI");

			PAINTSTRUCT ps = {};
			HDC hdc = BeginPaint(hwnd, &ps);
			{
				//FillRect(hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

				HGDIOBJ hof = SelectObject(hdc, hFont);
				SetBkMode(hdc, TRANSPARENT);
				SetTextColor(hdc, RGB(255, 255, 255));
				DrawTextW(hdc, L"SteelSeries H Wireless", -1, &rc, DT_CENTER | DT_SINGLELINE | DT_VCENTER);
				SelectObject(hdc, hof);
			}
			EndPaint(hwnd, &ps);

			return 0;;
		}
	}

	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}