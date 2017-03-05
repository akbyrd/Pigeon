#define WIN32_LEAN_AND_MEAN
// TODO: #include <minwindef.h>?
#include <Windows.h>
// TODO: Switch to WRL ComPtr
#include <atlbase.h> //CComPtr

#include "shared.hpp"
#include "notification.hpp"
#include "audio.hpp"
#include "video.hpp"

static const c16* PIGEON_GUID = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";
static const c16* SINGLE_INSTANACE_MUTEX_NAME = L"Pigeon Single Instance Mutex";
static const c16* NEW_PROCESS_MESSAGE_NAME = L"Pigeon New Process Name";

struct Hotkey
{
	i32 id;
	u32 modifier;
	u32 key;
};

b32 ReleaseHotkeys(NotificationWindow*, Hotkey*, u32, HANDLE);

int CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
	/* TODO: Overhaul error handling
	 * General policy: if the program runs, it will have full functionality
	 * 
	 * - Option 1: Nested ifs, inline
	 *    Pros: 'simple'
	 *    Cons: Harder to read, separates error location and handling
	 * 
	 * - Option 2: Nested ifs, functions
	 *    Pros: Keeps error location somewhat closer to occurrence location
	 *    Cons: Harder to read, jumping back and forth through (use-once) functions
	 * 
	 * - Option 3: State booleans
	 *    Pros: Keeps code linear
	 *    Cons: Maybe harder to follow? Combinatorial state to keep in head
	 * 
	 * - Option 4: Stage array or boolean
	 *    Pros: Keeps code linear; non-combinatorial
	 *    Cons: Slightly less obvious than nested ifs, still need nested ifs for smaller steps
	 *          (or ending up with a ton of enum values [probably too much overhead])
	 *    Impl: loop over enum values, switch, check index upon completion?
	 *    Impl: Check enum before each block, set enum when done
	 * 
	 * - Option 4: Local functions!
	 *    Pros: Keeps code linear, no large jumps in code, no nested ifs at all
	 *    Cons: Boilerplate code, stepping through with the debugger is a bit unintuitive
	 *    Impl: lambda functions
	 *    Impl: Compare the generated assembly for a lambdas a regular function in a sandbox project
	 *    Impl: Auto-capture by reference? (Does this fuck up the generated code?)
	 */

	// TODO: Show warning, show another warning while first is hiding => shows 2 warnings (repeating the first?)
	// TODO: Hotkeys don't work in fullscreen apps (e.g. Darksiders 2)
	// TODO: Pigeon image on startup
	// TODO: Pigeon SFX
	// TODO: SetProcessDPIAware?
	// TODO: Use a different animation timing method. SetTimer is not precise enough (rounds to multiples of 15.6ms)
	// TODO: Decouple errors and application closing
	// TODO: FormatMessage is not always giving good string translations

	// TODO: Hotkey to restart
	// TODO: Refactor animation stuff
	// TODO: Sound doesn't play on most devices when cycling audio devices
	// TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few seconds)
	// TODO: Integrate volume ducking?
	// https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
	// TODO: Auto-detect headset being turned on/off
	// TODO: Test with mutliple users. Might need use Local\ namespace for the event


	// Notification
	NotificationWindow notification = {};
	{
		// NOTE: QPC and QPF are documented as not being able to fail on XP+
		LARGE_INTEGER win32_tickFrequency = {};
		QueryPerformanceFrequency(&win32_tickFrequency);

		f64 tickFrequency = (f64) win32_tickFrequency.QuadPart;

		notification.windowMinWidth   = 200;
		notification.windowMaxWidth   = 600;
		notification.windowSize       = {200, 60};
		notification.windowPosition   = { 50, 60};
		notification.backgroundColor  = RGBA(16, 16, 16, 242);
		notification.textColorNormal  = RGB(255, 255, 255);
		notification.textColorError   = RGB(255, 0, 0);
		notification.textColorWarning = RGB(255, 255, 0);
		notification.textPadding      = 20;
		notification.animShowTicks    = 0.1 * tickFrequency;
		notification.animIdleTicks    = 2.0 * tickFrequency;
		notification.animHideTicks    = 1.0 * tickFrequency;
		notification.animUpdateMS     = 1000 / 30;
		notification.timerID          = 1;
		notification.tickFrequency    = tickFrequency;

		// DEBUG
		Notify(&notification, L"Started!");
	}


	// Window
	// TODO: Rename
	b32 initSuccess = [&]()
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0; //CS_DROPSHADOW
		windowClass.lpfnWndProc   = NotificationWndProc;
		windowClass.cbClsExtra    = 0;
		windowClass.cbWndExtra    = sizeof(&notification);
		windowClass.hInstance     = hInstance;
		windowClass.hIcon         = nullptr;
		windowClass.hCursor       = nullptr;
		windowClass.hbrBackground = nullptr;
		windowClass.lpszMenuName  = nullptr;
		windowClass.lpszClassName = L"Pigeon Notification Class";

		ATOM classAtom = RegisterClassW(&windowClass);
		if (classAtom == INVALID_ATOM)
		{
			NotifyWindowsError(&notification, L"RegisterClassW failed");
			return false;
		}

		notification.hwnd = CreateWindowExW(
			WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT,
			windowClass.lpszClassName,
			L"Pigeon Notification Window",
			WS_POPUP,
			notification.windowPosition.x,
			notification.windowPosition.y,
			notification.windowSize.cx,
			notification.windowSize.cy,
			nullptr,
			nullptr,
			hInstance,
			&notification
		);
		if (notification.hwnd == INVALID_HANDLE_VALUE)
		{
			NotifyWindowsError(&notification, L"CreateWindowExW failed");
			return false;
		}

		return true;
	}();


	/* TODO: Still have some failure cases
	 * - If an older process reaches the wait after a newer process it's going to get stuck there.
	 * - If a newer process posts a message before an older process creates its window and the
	 *   older process acquires the mutex from an even older process it will hang (until a new
	 *   message is posted)
	 */
	// Single Instance
	u64    startTime           = 0;
	u32    WM_NEWINSTANCE      = 0;
	HANDLE singleInstanceMutex = nullptr;
	initSuccess = initSuccess && [&]()
	{
		HANDLE hProcess = GetCurrentProcess();

		FILETIME win32_startFileTime = {};
		FILETIME unusued;
		b32 success = GetProcessTimes(hProcess, &win32_startFileTime, &unusued, &unusued, &unusued);
		if (!success)
		{
			NotifyWindowsError(&notification, L"GetProcessTimes failed");
			return false;
		}

		ULARGE_INTEGER win32_startTime = {};
		win32_startTime.LowPart  = win32_startFileTime.dwLowDateTime;
		win32_startTime.HighPart = win32_startFileTime.dwHighDateTime;

		startTime = win32_startTime.QuadPart;

		WM_NEWINSTANCE = RegisterWindowMessageW(NEW_PROCESS_MESSAGE_NAME);
		if (WM_NEWINSTANCE == 0)
		{
			NotifyWindowsError(&notification, L"RegisterWindowMessage failed");
			return false;
		}

		// TOOD: Namespace?
		singleInstanceMutex = CreateMutexW(nullptr, true, SINGLE_INSTANACE_MUTEX_NAME);
		if (!singleInstanceMutex)
		{
			NotifyWindowsError(&notification, L"CreateMutex failed");
			return false;
		}

		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// TODO: Better to enumerate processes instead of broadcasting
			success = PostMessageW(HWND_BROADCAST, WM_NEWINSTANCE, startTime, 0);
			if (!success)
			{
				NotifyWindowsError(&notification, L"PostMessage failed");
				return false;
			}

			// TODO: Not handling messages while waiting
			u32 uResult = WaitForSingleObject(singleInstanceMutex, INFINITE);
			if (uResult == WAIT_FAILED)
			{
				singleInstanceMutex = nullptr;

				NotifyWindowsError(&notification, L"WaitForSingleObject WAIT_FAILED");
				return false;
			}

			if (uResult == WAIT_ABANDONED) NotifyWindowsError(&notification, L"WaitForSingleObject WAIT_ABANDON", Error::Warning);
		}

		return true;
	}();


	// Misc
	initSuccess = initSuccess && [&]()
	{
		b32 success = InitializeAudio(&notification);
		if (!success) return false;

		success = SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		if (!success) NotifyWindowsError(&notification, L"SetPriorityClass failed", Error::Warning);
	}();


	// Hotkeys
	const i32           cycleAudioDeviceHotkeyID = 0;
	const i32        openPlaybackDevicesHotkeyID = 1;
	const i32           cycleRefreshRateHotkeyID = 2;
	const i32 openDisplayAdapterSettingsHotkeyID = 3;
	const i32              debug_MessageHotkeyID = 4;
	const i32              debug_WarningHotkeyID = 5;
	const i32                debug_ErrorHotkeyID = 6;

	//TODO: Probably just use lambdas
	Hotkey hotkeys[] = {
		{           cycleAudioDeviceHotkeyID,           0, VK_F5  },
		{        openPlaybackDevicesHotkeyID, MOD_CONTROL, VK_F5  },
		{           cycleRefreshRateHotkeyID,           0, VK_F6  },
		{ openDisplayAdapterSettingsHotkeyID, MOD_CONTROL, VK_F6  },
		{              debug_MessageHotkeyID,           0, VK_F10 },
		{              debug_WarningHotkeyID,           0, VK_F11 },
		{                debug_ErrorHotkeyID,           0, VK_F12 },
	};

	initSuccess = initSuccess && [&]()
	{
		for (u8 i = 0; i < ArrayCount(hotkeys); i++)
		{
			b32 success = RegisterHotKey(nullptr, hotkeys[i].id, MOD_WIN | MOD_NOREPEAT | hotkeys[i].modifier, hotkeys[i].key);
			if (!success)
			{
				NotifyWindowsError(&notification, L"RegisterHotKey failed");
				ReleaseHotkeys(&notification, hotkeys, ArrayCount(hotkeys), singleInstanceMutex);
				return false;
			}
		}

		return true;
	}();


	// Message pump
	MSG msg = {};
	b32 quit = false;

	while (!quit)
	{
		i32 iResult = GetMessageW(&msg, nullptr, 0, 0);
		if (iResult == -1)
		{
			// TODO: Log error
			break;
		}

		// TODO: Handle return values?
		TranslateMessage(&msg);
		DispatchMessageW(&msg);

		if (msg.message == WM_NEWINSTANCE)
		{
			u64 newProcessStartTime = msg.wParam;

			// TODO: This will lead to a hang if 2 processes have the exact same start time (include Process/ThreadID, highest wins?)
			if (newProcessStartTime > startTime)
			{
				if (singleInstanceMutex)
				{
					ReleaseHotkeys(&notification, hotkeys, ArrayCount(hotkeys), singleInstanceMutex);
				}

				notification.windowPosition.y += notification.windowSize.cy + 10;
				UpdateWindowPositionAndSize(&notification);

				Notify(&notification, L"There can be only one!", Error::Error);
			}
		}
		else
		{
			switch (msg.message)
			{
				case WM_HOTKEY:
				{
					switch (msg.wParam)
					{
						case cycleAudioDeviceHotkeyID:
							CycleDefaultAudioDevice(&notification);
							break;

						case openPlaybackDevicesHotkeyID:
							OpenAudioPlaybackDevicesWindow();
							break;

						case cycleRefreshRateHotkeyID:
							CycleRefreshRate(&notification);
							break;

						case openDisplayAdapterSettingsHotkeyID:
							OpenDisplayAdapterSettingsWindow();
							break;

						case debug_MessageHotkeyID:
							Notify(&notification, L"DEBUG Message", Error::None);
							break;

						case debug_WarningHotkeyID:
							Notify(&notification, L"DEBUG Warning", Error::Warning);
							break;

						case debug_ErrorHotkeyID:
							Notify(&notification, L"DEBUG Error", Error::Error);
							break;
					}
					break;
				}

				case WM_QUIT:
					quit = true;
					break;

				// Expected messages
				case WM_TIMER:
				case WM_PROCESSQUEUE:
					break;

				// TODO: WM_FONTCHANGE (29) - when installing fonts
				// TODO: WM_TIMECHANGE (30) - Shows up somewhat randomly
				// TODO: WM_KEYDOWN/UP (256, 257) - Somehow we're getting key messages, but only sometimes
				default:
					if (msg.message < WM_PROCESSQUEUE)
						NotifyFormat(&notification, L"Unexpected message: %d\n", Error::Warning, msg.message);
					break;
			}
		}
	}


	// Cleanup
	TeardownAudio();


	// TODO: Show remaining warnings / errors in a dialog?

	// Leak all the things!
	// (Windows destroys everything automatically)

	// NOTE: Handles are closed when process terminates.
	// Events are destroyed when the last handle is destroyed.

	// TODO: Can the mutex be abandoned before hotkeys are unregistered?

	// TODO: This is wrong
	return LOWORD(msg.wParam);
}

b32
ReleaseHotkeys(NotificationWindow* notification, Hotkey* hotkeys, u32 hotkeyCount, HANDLE mutex)
{
	for (u8 i = 0; i < hotkeyCount; i++)
	{
		b32 success = UnregisterHotKey(nullptr, hotkeys[i].id);
		if (!success)
		{
			NotifyWindowsError(notification, L"UnregisterHotKey failed", Error::Warning);
			return false;
		}
	}

	b32 success = ReleaseMutex(mutex);
	if (!success)
	{
		NotifyWindowsError(notification, L"ReleaseMutex failed", Error::Warning);
		return false;
	}

	mutex = nullptr;

	return true;
}