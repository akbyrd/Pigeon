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

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	int success;
	HRESULT hr;

	//TODO: Integrate volume ducking?
	//https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	//TODO: Play sound on switch?
	//TODO: Icon
	//TODO: Detect other instances, close them
	//TODO: Log failures
	//TODO: Use RawInput to get hardware key so it's not Logitech/Corsair profile dependent?
	//TODO: Auto-detect headset being turned on/off
	//TODO: Command line usage

	hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED | COINIT_SPEED_OVER_MEMORY);
	if (FAILED(hr)) return false;

	const int toggleAudioDeviceHotkeyID = 0;
	success = RegisterHotKey(nullptr, toggleAudioDeviceHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F5);
	if (!success) goto Cleanup;

	const int toggleRefreshRateHotkeyID = 0;
	success = RegisterHotKey(nullptr, toggleRefreshRateHotkeyID, MOD_WIN | MOD_NOREPEAT, VK_F6);
	if (!success) goto Cleanup;

	MSG msg;
	while (GetMessageW(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);

		switch (msg.message)
		{
			case WM_HOTKEY:
			{
				if (msg.wParam == toggleAudioDeviceHotkeyID)
					CycleDefaultAudioDevice();

				if (msg.wParam == toggleRefreshRateHotkeyID)
					CycleRefreshRate();

				break;
			}

			default:
				DebugPrint(L"Unexpected message: %d", msg.message);
		}
	}

	Cleanup:
	CoUninitialize();

	return 0;
}