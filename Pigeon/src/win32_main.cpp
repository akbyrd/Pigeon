#include <Windows.h>
#include <atlbase.h>

#include "shared.hpp"
#include "audio.hpp"
#include "video.hpp"

//TODO: Move to resources?
static const c16* GUIDSTR_PIGEON = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	b32 success;
	HRESULT hr;
	u32 result;
	MSG msg = {};


	//TODO: Display notification on change (built-in? custom?)
	//TODO: Shift to open to the relevant settings window
	//      %systemroot%\system32\rundll32.exe display.dll,ShowAdapterSettings
	//      Starts on first tab (Adapter)
	//      Run as child process, send commands to the window to change the tab?
	//TODO: Sound doesn't play on most devices when cycling audio devices
	//TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	//TODO: Custom sound?
	//TODO: Integrate volume ducking?
	//https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	//TODO: Log failures
	//TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	//TODO: Auto-detect headset being turned on/off
	//TODO: Test with mutliple users. Might need use Local\ namespace for the event

	//NOTE: Handles are closed when process terminates.
	//Events are destroyed when the last handle is destroyed.
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

	//TODO: BELOW_NORMAL_PRIORITY_CLASS?
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

	//TODO: These may be unnecessary, but I don't know what guarantees Windows
	//makes about when SetEvent will cause waiting threads to release. If the
	//release happens immediately, the hotkeys need to be unregistered first.
	UnregisterHotKey(nullptr, cycleRefreshRateHotkeyID);
	UnregisterHotKey(nullptr, cycleAudioDeviceHotkeyID);
	SetEvent(singleInstanceEvent);

	//TODO: This is wrong
	return LOWORD(msg.wParam);
}

inline LRESULT CALLBACK
WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}