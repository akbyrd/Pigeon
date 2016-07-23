#include <Windows.h>
#include <atlbase.h>

#include <cstdint>
typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float  f32;
typedef double f64;

typedef char    c8;
typedef wchar_t c16;

typedef int b32;

typedef size_t index;

#define ArrayCount(x) sizeof(x) / sizeof(x[0])

#include <cstdio>
inline void
DebugPrint(c16 const* format, ...)
{
	int result;

	va_list args;
	va_start (args, format);
	c16 buffer[128];
	result = _vsnwprintf_s(buffer, ArrayCount(buffer), _TRUNCATE, format, args);
	OutputDebugStringW(buffer);
	va_end (args);
}

#include "audio.hpp"
#include "video.hpp"

static const c16* GUIDSTR_PIGEON = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	b32 success;
	HRESULT hr;
	u32 result;
	MSG msg = {};


	//TODO: Sound doesn't play on most devices when cycling audio devices
	//TODO: Shift to open to the relevant settings window
	//TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	//TODO: Custom sound?
	//TODO: Integrate volume ducking?
	//https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	//TODO: Display notification on change
	//TODO: Log failures
	//TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	//TODO: Auto-detect headset being turned on/off
	//TODO: Command line usage
	//TODO: Test with mutliple users. Might need use Local\ namespace for the event
	//TODO: Solve IPolicyConfig GUID issue

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

	const int cycleRefreshRateHotkeyID = 1;
	success = RegisterHotKey(nullptr, cycleRefreshRateHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

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

						case cycleRefreshRateHotkeyID:
							CycleRefreshRate();
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
	//makes about when SetEvent will cause waiting threads to release.
	UnregisterHotKey(nullptr, cycleRefreshRateHotkeyID);
	UnregisterHotKey(nullptr, cycleAudioDeviceHotkeyID);
	SetEvent(singleInstanceEvent);

	//TODO: This is wrong
	return LOWORD(msg.wParam);
}