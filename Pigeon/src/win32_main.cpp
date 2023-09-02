#define WIN32_LEAN_AND_MEAN
// TODO: #include <minwindef.h>?
#include <Windows.h>
#include <shellapi.h>
#include <Shlobj.h>
#include <Tpcshrd.h>
// TODO: Switch to WRL ComPtr
#include <atlbase.h> // CComPtr

#include "shared.hpp"
#include "notification.hpp"
// TODO: Yuck.
b32 RunCommand(NotificationWindow*, c8*);
#include "audio.hpp"
#include "video.hpp"
#include "WindowMessageStrings.h"

// TODO: Would it be better to refactor the Notify process to be able to reserve the next spot, fill
// the buffer, then process the notification?

// TODO: BUG: Show warning, show another warning while first is hiding => shows 2 warnings
// (repeating the first?)
// TODO: Look for a way to start faster at login (using Startup folder seems to take quite a few
// seconds)
// TODO: Hotkeys don't work in fullscreen apps (e.g. Darksiders 2)
// TODO: SetProcessDPIAware?
// TODO: Minimize the number of link dependencies

// TODO: Pigeon image on startup
// TODO: Pigeon SFX
// TODO: Line on notification indicating queue count (colored if warning/error exists?)

// TODO: Hotkey to restart
// TODO: Hotkey to show next notification
// TODO: Hotkey to clear all notifications
// TODO: Refactor animation stuff
// TODO: Use a different animation timing method. SetTimer is not precise enough (rounds to
// multiples of 15.6ms)
// TODO: Integrate volume ducking?
// https://msdn.microsoft.com/en-us/library/windows/desktop/dd940522(v=vs.85).aspx
// TODO: Auto-detect headset being turned on/off
// TODO: Test with mutliple users. Might need use Local\ namespace for the event

static const c16* PIGEON_GUID = L"{C1FA11EF-FC16-46DF-A268-104F59E94672}";
static const c16* SINGLE_INSTANCE_MUTEX_NAME = L"Pigeon Single Instance Mutex";
static const c16* NEW_PROCESS_MESSAGE_NAME = L"Pigeon New Process Name";

enum struct InitPhase
{
	None,
	WindowCreated,
	SingleInstanceEnforced,
	HotkeysRegistered,
	SystemsInitialized,
	LogFileCreated,
};

struct Hotkey
{
	u32 modifier;
	u32 key;
	b32 (*execute)(NotificationWindow*);

	i32 id;
	b32 registered;
};

b32
Initialize(
	InitPhase& phase,
	NotificationWindow& notification,
	HINSTANCE hInstance,
	u64& startTime,
	u32& processID,
	u32& WM_NEWINSTANCE,
	HANDLE& singleInstanceMutex,
	Hotkey* hotkeys,
	u8 hotkeyCount,
	HANDLE& logFile)
{
	// Create window
	{
		WNDCLASSW windowClass = {};
		windowClass.style         = 0;
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
			notification.hwnd = nullptr;
			NotifyWindowsError(&notification, L"CreateWindowExW failed");
			return false;
		}

		phase = InitPhase::WindowCreated;
	}


	// Enforce single instance
	{
		// TODO: Still have some failure cases
		// - If an older process reaches the wait after a newer process it's going to get stuck there.
		// - If a newer process posts a message before an older process creates its window and the
		//   older process acquires the mutex from an even older process it will hang (until a new
		//   message is posted)

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

		processID = GetProcessId(hProcess);

		WM_NEWINSTANCE = RegisterWindowMessageW(NEW_PROCESS_MESSAGE_NAME);
		if (WM_NEWINSTANCE == 0)
		{
			NotifyWindowsError(&notification, L"RegisterWindowMessage failed");
			return false;
		}

		// TODO: Namespace?
		singleInstanceMutex = CreateMutexW(nullptr, true, SINGLE_INSTANCE_MUTEX_NAME);
		if (!singleInstanceMutex)
		{
			NotifyWindowsError(&notification, L"CreateMutex failed");
			return false;
		}

		if (GetLastError() == ERROR_ALREADY_EXISTS)
		{
			// TODO: Better to enumerate processes instead of broadcasting
			success = PostMessageW(HWND_BROADCAST, WM_NEWINSTANCE, startTime, processID);
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

			if (uResult == WAIT_ABANDONED) NotifyWindowsError(&notification, L"WaitForSingleObject WAIT_ABANDON", Severity::Warning);
		}

		phase = InitPhase::SingleInstanceEnforced;
	}


	// Register hotkeys
	{
		for (u8 i = 0; i < hotkeyCount; i++)
		{
			b32 success = RegisterHotKey(nullptr, hotkeys[i].id, MOD_WIN | MOD_NOREPEAT | hotkeys[i].modifier, hotkeys[i].key);
			if (!success)
			{
				NotifyWindowsError(&notification, L"RegisterHotKey failed");

				b32 UnregisterHotkeys(NotificationWindow&, Hotkey*, u8, HANDLE&);
				success = UnregisterHotkeys(notification, hotkeys, hotkeyCount, singleInstanceMutex);
				if (!success) phase = InitPhase::HotkeysRegistered;

				return false;
			}

			hotkeys[i].registered = true;
		}

		phase = InitPhase::HotkeysRegistered;
	}


	// Initialize systems
	{
		HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
		if (FAILED(hr))
		{
			NotifyWindowsError(&notification, L"CoInitializeEx failed", Severity::Error, hr);
			return false;
		}

		phase = InitPhase::SystemsInitialized;
	}


	// Create log file
	{
		c16* logFolderPath = nullptr;
		HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr, &logFolderPath);
		if (FAILED(hr))
		{
			NotifyWindowsError(&notification, L"SHGetFolderPath failed");
			return false;
		}

		swprintf(notification.logFilePath, ArrayCount(notification.logFilePath), L"%s\\Pigeon", logFolderPath);
		b32 result = CreateDirectoryW(notification.logFilePath, nullptr);
		if (!result)
		{
			u32 error = GetLastError();
			if (error != ERROR_ALREADY_EXISTS)
			{
				NotifyWindowsError(&notification, L"Failed to create log file path", Severity::Warning, error);
				return false;
			}
		}

		wcscat_s(notification.logFilePath, ArrayCount(notification.logFilePath), L"\\pigeon.log");

		logFile = CreateFileW(
			notification.logFilePath,
			GENERIC_WRITE,
			// TODO: Test write and delete while file is open
			FILE_SHARE_READ | FILE_SHARE_DELETE,
			nullptr,
			// TODO: Change this back to CREATE_ALWAYS at some point
			OPEN_ALWAYS,
			//CREATE_ALWAYS,
			FILE_ATTRIBUTE_NORMAL,
			nullptr
		);
		if (logFile == INVALID_HANDLE_VALUE)
		{
			notification.logFilePath[0] = '\0';
			NotifyWindowsError(&notification, L"CreateFile failed", Severity::Warning);
			return false;
		}

		phase = InitPhase::LogFileCreated;
	}


	return true;
}

b32
OpenLogFile(NotificationWindow* notification)
{
	#define NOTIFY_IF(expression, string, reaction) \
		if (expression) \
		{ \
			Notify(notification, string, Severity::Warning); \
			reaction; \
		} \


	NOTIFY_IF(!notification->logFilePath[0], L"No log file", return false);

	// The return value is treated as an int. It's not a real HINSTANCE
	HINSTANCE result = ShellExecuteW(
		nullptr,
		L"open",
		notification->logFilePath,
		nullptr,
		nullptr,
		SW_SHOW);
	NOTIFY_IF((i64) result < 32, L"ShellExecuteW failed", return false);

	return true;
}

b32
UnregisterHotkeys(NotificationWindow& notification, Hotkey* hotkeys, u8 hotkeyCount, HANDLE& singleInstanceMutex)
{
	b32 unregisterFailed = false;
	for (u8 i = 0; i < hotkeyCount; i++)
	{
		if (hotkeys[i].registered)
		{
			b32 success = UnregisterHotKey(nullptr, hotkeys[i].id);
			if (!success)
			{
				unregisterFailed = true;
				NotifyWindowsError(&notification, L"UnregisterHotKey failed", Severity::Warning);
				continue;
			}

			hotkeys[i].registered = false;
		}
	}
	if (unregisterFailed) return false;


	b32 success = ReleaseMutex(singleInstanceMutex);
	if (!success)
	{
		NotifyWindowsError(&notification, L"ReleaseMutex failed", Severity::Warning);
		return false;
	}

	singleInstanceMutex = nullptr;

	return true;
};

b32
RunCommand(NotificationWindow* notification, c8* command)
{
	u32 uResult = WinExec(command, SW_NORMAL);
	if (uResult < 32)
	{
		// TODO: Can we get an error message from the result value?
		NotifyFormat(notification, L"WinExec failed: %u", Severity::Warning, uResult);
		return false;
	}

	return true;
}

i32 CALLBACK
wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, i32 nCmdShow)
{
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

		// DEBUG:
		Notify(&notification, L"Started!");
	}


	Hotkey hotkeys[] = {
		//{           0, VK_F8 , nullptr                           },
		{ MOD_CONTROL, VK_F8 , &OpenLogFile                      },
		{           0, VK_F9 , &CycleAudioPlaybackDevice         },
		{ MOD_CONTROL, VK_F9 , &OpenAudioPlaybackDevicesWindow   },
		{           0, VK_F10, &CycleAudioRecordingDevice        },
		{ MOD_CONTROL, VK_F10, &OpenAudioRecordingDevicesWindow  },
		{           0, VK_F11, &SetMaximumRefreshRate            },
		{ MOD_CONTROL, VK_F11, &OpenDisplayAdapterSettingsWindow },
		{           0, VK_F12, &ToggleMuteForCurrentApplication  },
		{ MOD_CONTROL, VK_F12, &OpenVolumeMixerWindow            },

		#if false
		#define LAMBDA(x) [](NotificationWindow* notification) -> b32 { x; return true; }
		{           0, VK_F9 , LAMBDA(Notify(notification, L"DEBUG Message", Severity::Info))    },
		{           0, VK_F10, LAMBDA(Notify(notification, L"DEBUG Warning", Severity::Warning)) },
		{           0, VK_F11, LAMBDA(Notify(notification, L"DEBUG Error"  , Severity::Error))   },
		{           0, VK_F12, &RestartApplication                                               },
		#undef LAMBDA
		#endif
	};

	for (i32 i = 0; i < ArrayCount(hotkeys); i++)
		hotkeys[i].id = i;


	// Initialize
	InitPhase phase = InitPhase::None;

	u64    startTime           = 0;
	u32    processID           = 0;
	u32    WM_NEWINSTANCE      = 0;
	HANDLE singleInstanceMutex = nullptr;
	HANDLE logFile             = nullptr;

	Initialize(phase,
		notification, hInstance,
		startTime, processID, WM_NEWINSTANCE, singleInstanceMutex,
		hotkeys, ArrayCount(hotkeys), logFile
	);


	// Misc
	b32 success = SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
	if (!success) NotifyWindowsError(&notification, L"SetPriorityClass failed", Severity::Warning);


	// Message loop
	i32 returnValue = -1;
	if (phase >= InitPhase::WindowCreated)
	{
		MSG msg = {};
		b32 quit = false;

		while (!quit)
		{
			// NOTE: Unfiltered GetMessage won't fail
			// https://blogs.msdn.microsoft.com/oldnewthing/20130322-00/?p=4873
			GetMessageW(&msg, nullptr, 0, 0);

			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			// TODO: This causes an interesting queue overflow scenario. Probably related to the bug in
			// the todo list
			//NotifyFormat(&notification, L"Ima overflowin' ur bufferz", Severity::Warning);

			if (msg.message == WM_NEWINSTANCE)
			{
				u64 newProcessStartTime = msg.wParam;
				u32 newProcessID        = (u32) msg.lParam;

				if (newProcessStartTime > startTime
				 || (newProcessStartTime == startTime && newProcessID > processID))
				{
					if (singleInstanceMutex)
					{
						UnregisterHotkeys(notification, hotkeys, ArrayCount(hotkeys), singleInstanceMutex);
					}

					notification.windowPosition.y += notification.windowSize.cy + 10;
					UpdateWindowPositionAndSize(&notification);

					Notify(&notification, L"There can be only one!", Severity::Error);
				}
			}
			else
			{
				switch (msg.message)
				{
					case WM_HOTKEY:
					{
						for (u8 i = 0; i < ArrayCount(hotkeys); i++)
						{
							if (msg.wParam == hotkeys[i].id)
							{
								hotkeys[i].execute(&notification);
								break;
							}
						}
						break;
					}

					case WM_QUIT:
						quit = true;
						returnValue = (i32) msg.wParam;
						break;

					// Expected messages
					case WM_TIMER:
					case WM_TIMECHANGE:
					case WM_PROCESSQUEUE:
					case WM_TABLET_ADDED:
					case WM_TABLET_DELETED:
					case WM_FONTCHANGE:
					case WM_DWMCOLORIZATIONCOLORCHANGED:
					// Undocumented, happens when toggling high contrast mode. Closest to WM_THEMECHANGED (0x31A)
					case 0x31B:
						break;

					// NOTE: Use msg.message,wm in the Watch window to see the message name!
					default:
					{
						if (msg.message < WM_PROCESSQUEUE)
						{
							c16* messageName = GetWindowMessageName(msg.message);
							if (!messageName)
							{
								// TODO: Check error
								c16 buffer[32];
								swprintf(buffer, ArrayCount(buffer), L"UNKNOWN (0x%1X)", msg.message);

								messageName = buffer;
							}

							NotifyFormat(&notification, L"Unexpected message: %s, w:0x%1llX", Severity::Warning, messageName, msg.wParam);

							c16 log[32];
							i32 logLen = swprintf(log, ArrayCount(log), L"UNKNOWN (0x%1X), w:0x%1llX\n", msg.message, msg.wParam);
							if (logLen < 0)
							{
								NotifyFormat(&notification, L"Log format failed", Severity::Warning);
								break;
							}

							// NOTE: This still succeeds if the file is deleted. Strange.
							i32 logBytes = logLen * sizeof(log[0]);
							b32 result = WriteFile(logFile, log, logBytes, nullptr, nullptr);
							if (!result)
								NotifyFormat(&notification, L"Log write failed", Severity::Warning);
						}
						break;
					}
				}
			}
		}
	}


	// Cleanup
	if (phase >= InitPhase::LogFileCreated)
		CloseHandle(logFile);

	if (phase >= InitPhase::SystemsInitialized)
		// TODO: This can enter a modal loop and dispatch messages. Understand the implications of
		// this.
		CoUninitialize();

	// Show remaining errors
	for (u8 i = 0; i < notification.queueCount; i++)
	{
		u8 actualIndex = LogicalToActualIndex(&notification, i);
		Notification note = notification.queue[actualIndex];

		if (note.severity == Severity::Error)
		{
			// NOTE: This will block until Ok is clicked, but that's ok because it only happens when
			// window creation failed and we don't hold the mutex.
			// TODO: Ocassionally getting this with "There can be only one!" if running a shit ton of
			// instances
			i32 iResult = MessageBoxW(nullptr, note.text, L"Pigeon Error", MB_OK | MB_ICONERROR | MB_SERVICE_NOTIFICATION);
			if (iResult == 0) {} // TODO: Uh, system log?
		}
	}

	// Leak all the things!
	// (Windows destroys everything automatically)

	// NOTE: Handles are closed when process terminates.
	// Events are destroyed when the last handle is destroyed.

	// TODO: Can the mutex be abandoned before hotkeys are unregistered?

	return returnValue;
}
