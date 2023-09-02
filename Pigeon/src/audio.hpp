#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <playsoundapi.h>
#include <propkey.h>
#include <tlhelp32.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "IPolicyConfig.h"

enum struct AudioType
{
	Null,
	Playback,
	Recording,
};

b32
CycleAudioDevice(NotificationWindow* notification, AudioType audioType)
{
	// NOTE: CoInitialize is assumed to have been called.
	HRESULT hr;


	#define NOTIFY_IF(expression, string, reaction) \
		if (expression) \
		{ \
			Notify(notification, string, Severity::Warning); \
			reaction; \
		} \

	#define NOTIFY_IF_FAILED(string, hr, reaction) \
		if (FAILED(hr)) \
		{ \
			NotifyWindowsError(notification, string, Severity::Warning, hr); \
			reaction; \
		}


	// Shared
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	NOTIFY_IF_FAILED(L"CoCreateInstance failed", hr, return false);

	EDataFlow dataFlow = (EDataFlow) -1;
	switch (audioType)
	{
		default: Assert(false); break;
		case AudioType::Playback:  dataFlow = EDataFlow::eRender;  break;
		case AudioType::Recording: dataFlow = EDataFlow::eCapture; break;
	}


	// Check for devices
	CComPtr<IMMDeviceCollection> deviceCollection;
	u32 deviceCount;
	{
		hr = deviceEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &deviceCollection);
		NOTIFY_IF_FAILED(L"EnumAudioEndpoints failed", hr, return false);

		hr = deviceCollection->GetCount(&deviceCount);
		NOTIFY_IF_FAILED(L"GetCount failed", hr, return false);

		NOTIFY_IF(deviceCount == 0, L"No devices found", return true);
	}


	// Get current audio device
	CComHeapPtr<c16> currentDefaultDeviceID;
	{
		CComPtr<IMMDevice> currentDevice;
		hr = deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, ERole::eConsole, &currentDevice);
		NOTIFY_IF_FAILED(L"GetDefaultAudioEndpoint failed", hr, return false);

		hr = currentDevice->GetId(&currentDefaultDeviceID);
		NOTIFY_IF_FAILED(L"GetId failed", hr, return false);
	}


	// Find next available audio device
	CComHeapPtr<c16> newDefaultDeviceID;
	{
		b32 useNextDevice = false;
		for (u32 i = 0; i < deviceCount + 1; ++i)
		{
			u32 index = i % deviceCount;

			CComPtr<IMMDevice> device;
			hr = deviceCollection->Item(index, &device);
			NOTIFY_IF_FAILED(L"Item failed", hr, continue);

			CComHeapPtr<c16> deviceID;
			hr = device->GetId(&deviceID);
			NOTIFY_IF_FAILED(L"GetId failed", hr, continue);

			if (useNextDevice)
			{
				newDefaultDeviceID = deviceID;
				break;
			}

			if (wcscmp(deviceID, currentDefaultDeviceID) == 0)
			{
				useNextDevice = true;
			}
		}
	}


	// Notification
	{
		const wchar_t* genericNames[] = {
			L"Speaker",
			L"Headset",
			L"Headset Earphone",
			L"Headphones",
			L"Microphone",
		};

		CComPtr<IMMDevice> device;
		hr = deviceEnumerator->GetDevice(newDefaultDeviceID, &device);
		NOTIFY_IF_FAILED(L"GetDevice failed", hr, return false);

		CComPtr<IPropertyStore> propertyStore;
		hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
		NOTIFY_IF_FAILED(L"OpenPropertyStore failed", hr, return false);

		PROPVARIANT deviceDescription;
		PropVariantInit(&deviceDescription);

		hr = propertyStore->GetValue(PKEY_Device_DeviceDesc, &deviceDescription);
		NOTIFY_IF_FAILED(L"GetValue failed", hr, return false);

		for (const c16* genericName : genericNames)
		{
			if (wcscmp(genericName, deviceDescription.pwszVal) == 0)
			{
				hr = PropVariantClear(&deviceDescription);
				NOTIFY_IF_FAILED(L"PropVariantClear failed", hr, return false);

				PropVariantInit(&deviceDescription);

				hr = propertyStore->GetValue(PKEY_DeviceInterface_FriendlyName, &deviceDescription);
				NOTIFY_IF_FAILED(L"GetValue failed", hr, return false);

				break;
			}
		}

		Notify(notification, deviceDescription.pwszVal);

		hr = PropVariantClear(&deviceDescription);
		NOTIFY_IF_FAILED(L"PropVariantClear failed", hr, return false);
	}


	// Set next audio device
	{
		CComPtr<IPolicyConfig> policyConfig;
		hr = CoCreateInstance(CLSID_CPolicyConfigClient, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policyConfig));
		NOTIFY_IF_FAILED(L"CoCreateInstance failed", hr, return false);

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eConsole);
		NOTIFY_IF_FAILED(L"SetDefaultEndpoint failed", hr, return false);

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eMultimedia);
		NOTIFY_IF_FAILED(L"SetDefaultEndpoint failed", hr, return false);

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eCommunications);
		NOTIFY_IF_FAILED(L"SetDefaultEndpoint failed", hr, return false);


		if (audioType == AudioType::Playback)
		{
			b32 success = PlaySoundW((c16*) SND_ALIAS_SYSTEMDEFAULT, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SYSTEM);
			if (!success)
			{
				Notify(notification, L"PlaySound failed", Severity::Warning);
				return false;
			}
		}
	}

	return true;

	#undef NOTIFY_IF
	#undef NOTIFY_IF_FAILED
}

b32
CycleAudioPlaybackDevice(NotificationWindow* notification)
{
	// NOTE: CoInitialize is assumed to have been called.
	return CycleAudioDevice(notification, AudioType::Playback);
}

b32
OpenAudioPlaybackDevicesWindow(NotificationWindow* notification)
{
	c8 command[] = "control.exe /name Microsoft.Sound /page Playback";
	return RunCommand(notification, command);
}

b32
CycleAudioRecordingDevice(NotificationWindow* notification)
{
	// NOTE: CoInitialize is assumed to have been called.
	return CycleAudioDevice(notification, AudioType::Recording);
}

b32
OpenAudioRecordingDevicesWindow(NotificationWindow* notification)
{
	c8 command[] = "control.exe /name Microsoft.Sound /page Recording";
	return RunCommand(notification, command);
}

struct ProcessInfo
{
	u32 id;
	u32 rootId;
	c16 name[256];
};

void
GetProcessName(NotificationWindow* notification, HANDLE snapshot, ProcessInfo& process)
{
	// NOTE: CoInitialize is assumed to have been called.


	#define NOTIFY_IF(expression, string, reaction) \
		if (expression) \
		{ \
			Notify(notification, string, Severity::Warning); \
			reaction; \
		} \

	#define NOTIFY_IF_FAILED(string, hr, reaction) \
		if (FAILED(hr)) \
		{ \
			NotifyWindowsError(notification, string, Severity::Warning, hr); \
			reaction; \
		}


	// Open process returns 0, not INVALID_HANDLE_VALUE
	HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process.id);
	NOTIFY_IF(!processHandle, L"OpenProcess failed", return);
	defer { CloseHandle(processHandle); };

	c16 processPath[MAX_PATH];
	u32 pathLength = ArrayCount(processPath);
	b32 result = QueryFullProcessImageNameW(processHandle, 0, processPath, (DWORD*) &pathLength);
	NOTIFY_IF(!result, L"QueryFullProcessImageNameW failed", return);


	// Try the FileDescription property of the version info
	for (;;)
	{
		u32 versionInfoSize = GetFileVersionInfoSizeW(processPath, nullptr);
		if (!versionInfoSize) break;

		u8* versionInfo = new u8[versionInfoSize];
		defer { delete[] versionInfo; };
		result = GetFileVersionInfoW(processPath, 0, versionInfoSize, versionInfo);
		if (!result) break;

		// It's hard to know which language and code page to use when querying the file
		// description. Different programs use different combinations. Trying to match them
		// against the user's preferred lanaguage would be hairy, so we try the neutral
		// language, then fall back to first available combination.
		//
		// Languages
		// * 0000 - Neutral - Firefox, Spotify
		// * 0409 - en-US   - Discord, Steam, Edge, Epic
		// * 0009 -         - NVidia

		c16* tempDescription;
		u32 descriptionLength;

		// First try the neutral language
		c16 subBlock[] = L"\\StringFileInfo\\000004b0\\FileDescription";
		if (VerQueryValueW(versionInfo, subBlock, (void**) &tempDescription, &descriptionLength))
		{
			wcscpy_s(process.name, ArrayCount(process.name), tempDescription);
			return;
		}

		// Otherwise use the first available language
		struct Translation
		{
			u16 language;
			u16 codepage;
		};

		u32 translationSize;
		Translation* translations;
		b32 result = VerQueryValueW(versionInfo, L"\\VarFileInfo\\Translation", (void**) &translations, &translationSize);
		NOTIFY_IF(!result, L"GetFileVersionInfoSizeW failed", break);

		i32 translationCount = translationSize / sizeof(Translation);
		for (i32 i = 0; i < translationCount; i++)
		{
			Translation& translation = translations[i];

			c16 language[9];
			swprintf(language, ArrayCount(language), L"%04x%04x", translation.language, translation.codepage);
			wmemcpy(&subBlock[16], language, ArrayCount(language) - 1);

			// Some edge cases to be aware of:
			// * Some programs have an FileDescription but it's empty (Fusion 360, for example)
			// * I don't see an easy to to know if a codepage is UTF-16 compatible, so we take what we get and pray.

			if (VerQueryValueW(versionInfo, subBlock, (void**) &tempDescription, &descriptionLength))
			{
				if (descriptionLength != 0)
				{
					wcscpy_s(process.name, ArrayCount(process.name), tempDescription);
					return;
				}
			}
		}
	}


	// If we didn't find a file description (UWP applications) use the display name from the shell
	{
		CComPtr<IShellItem2> shellItem;
		HRESULT hr = SHCreateItemFromParsingName(processPath, nullptr, IID_PPV_ARGS(&shellItem));
		NOTIFY_IF_FAILED(L"SHCreateItemFromParsingName failed", hr, return);

		CComHeapPtr<c16> value;
		hr = shellItem->GetString(PKEY_ItemNameDisplayWithoutExtension, &value);
		NOTIFY_IF_FAILED(L"IShellItem2::GetString failed", hr, return);

		wcscpy_s(process.name, ArrayCount(process.name), value);
		return;
	}


	// Fallback to the executable name
	{
		PROCESSENTRY32W processEntry;
		processEntry.dwSize = sizeof(PROCESSENTRY32W);

		for (b8 Continue = Process32FirstW(snapshot, &processEntry);
				Continue;
				Continue = Process32NextW(snapshot, &processEntry))
		{
			if (processEntry.th32ProcessID == process.id)
			{
				wcscpy_s(process.name, ArrayCount(process.name), processEntry.szExeFile);
				return;
			}
		}
	}


	// Failure
	{
		swprintf(process.name, ArrayCount(process.name), L"%i", process.id);
		return;
	}

	#undef NOTIFY_IF
	#undef NOTIFY_IF_FAILED
}

void
GetProcessRoot(HANDLE snapshot, ProcessInfo& process)
{
	PROCESSENTRY32W processEntry;
	processEntry.dwSize = sizeof(PROCESSENTRY32W);

	u32 maybeParentProcessId = 0;
	process.rootId = process.id;

	for (b8 Continue = Process32FirstW(snapshot, &processEntry);
			Continue;
			Continue = Process32NextW(snapshot, &processEntry))
	{
		if (processEntry.th32ProcessID == process.id)
		{
			maybeParentProcessId = processEntry.th32ParentProcessID;
			break;
		}
	}

	// The system process has an id of 0 and a parent of 0. For that case, and
	while (maybeParentProcessId)
	{
		for (b8 Continue = Process32FirstW(snapshot, &processEntry);
			Continue;
			Continue = Process32NextW(snapshot, &processEntry))
		{
			if (processEntry.th32ProcessID == maybeParentProcessId)
			{
				// Some processes have explorer.exe as their parent. We don't want to consider
				// everything with explorer as a parent part of the same process so we stop the search.
				c16* ignoredProcesses[] = {
					L"explorer.exe",
					L"svchost.exe",
				};

				for (c16* ignoredProcess : ignoredProcesses)
				{
					if (wcscmp(processEntry.szExeFile, ignoredProcess) == 0)
					{
						maybeParentProcessId = 0;
						break;
					}
				}
				if (maybeParentProcessId == 0)
					break;

				// Just to be safe stop the search if we ever hit a loop where the parent id is the same
				// as the current id.
				if (processEntry.th32ParentProcessID == maybeParentProcessId)
				{
					maybeParentProcessId = 0;
					break;
				}

				process.rootId = maybeParentProcessId;
				maybeParentProcessId = processEntry.th32ParentProcessID;
				break;
			}
		}

		// It's possible for a process to have parent that does not appear in the list. I suspect this
		// is either an issue with permissions flags provided to CreateToolhelp32Snapshot.
		// Until/unless this becomes and issue in practice just abort the search.
		maybeParentProcessId = 0;
	}
}

b32
ToggleMuteForCurrentApplication(NotificationWindow* notification)
{
	// NOTE: CoInitialize is assumed to have been called.
	HRESULT hr;


	#define NOTIFY_IF(expression, string, reaction) \
		if (expression) \
		{ \
			Notify(notification, string, Severity::Warning); \
			reaction; \
		} \

	#define NOTIFY_IF_FAILED(string, hr, reaction) \
		if (FAILED(hr)) \
		{ \
			NotifyWindowsError(notification, string, Severity::Warning, hr); \
			reaction; \
		}

	#define NOTIFY_IF_INVALID_HANDLE(string, handle, reaction) \
		if (handle == INVALID_HANDLE_VALUE) \
		{ \
			Notify(notification, string, Severity::Warning); \
			reaction; \
		}


	HANDLE snapshot = INVALID_HANDLE_VALUE;
	defer { CloseHandle(snapshot); };
	{
		snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		NOTIFY_IF_INVALID_HANDLE(L"CreateToolhelp32Snapshot failed", snapshot, return false);
	}


	// Find the currently focused process
	ProcessInfo focusedProcess = {};
	{
		// GetForegroundWindow does not work for UWP applications.
		GUITHREADINFO threadInfo = {};
		threadInfo.cbSize = sizeof(GUITHREADINFO);
		b32 result = GetGUIThreadInfo(0, &threadInfo);
		NOTIFY_IF(!result, L"GetGUIThreadInfo failed", return false);

		HWND hwnd = threadInfo.hwndFocus ? threadInfo.hwndFocus : threadInfo.hwndActive;
		NOTIFY_IF(!hwnd, L"Failed to find focused window", return false);
		hr = GetWindowThreadProcessId(hwnd, (DWORD*) &focusedProcess.id);
		NOTIFY_IF_FAILED(L"GetWindowThreadProcessId failed", hr, return false);

		GetProcessName(notification, snapshot, focusedProcess);
		GetProcessRoot(snapshot, focusedProcess);
	}


	// Mute all audio streams associated with the process
	{
		CComPtr<IMMDeviceEnumerator> deviceEnumerator;
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
		NOTIFY_IF_FAILED(L"CoCreateInstance failed", hr, return false);

		CComPtr<IMMDevice> currentDevice;
		hr = deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &currentDevice);
		NOTIFY_IF_FAILED(L"GetDefaultAudioEndpoint failed", hr, return false);

		CComPtr<IAudioSessionManager2> sessionManager;
		hr = currentDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**) &sessionManager);
		NOTIFY_IF_FAILED(L"IMMDevice::Activate failed", hr, return false);

		CComPtr<IAudioSessionEnumerator> sessionList;
		hr = sessionManager->GetSessionEnumerator(&sessionList);
		NOTIFY_IF_FAILED(L"GetSessionEnumerator failed", hr, return false);

		int sessionCount;
		hr = sessionList->GetCount(&sessionCount);
		NOTIFY_IF_FAILED(L"IAudioSessionEnumerator::GetCount failed", hr, return false);

		b8 foundStream = false;
		b32 mute = true;

		for (int i = 0; i < sessionCount; i++)
		{
			CComPtr<IAudioSessionControl> sessionControl;
			hr = sessionList->GetSession(i, &sessionControl);
			NOTIFY_IF_FAILED(L"IAudioSessionEnumerator::GetSession failed", hr, return false);

			CComPtr<IAudioSessionControl2> sessionControl2;
			hr = sessionControl->QueryInterface(IID_PPV_ARGS(&sessionControl2));
			NOTIFY_IF_FAILED(L"IAudioSessionControl::QueryInterface failed", hr, return false);

			ProcessInfo process = {};
			hr = sessionControl2->GetProcessId((DWORD*) &process.id);
			NOTIFY_IF_FAILED(L"IAudioSessionControl2::GetProcessId failed", hr, return false);

			// TODO: Can this be done the other way around? Ask if stream process has a focused window

			// We use the root process because some applications
			// * Have a root process with an audio stream (Discord)
			// * Have audio streams and no windows (Discord, NVidia Container)
			GetProcessRoot(snapshot, process);
			if (process.rootId == focusedProcess.rootId)
			{
				CComPtr<ISimpleAudioVolume> audioVolume;
				hr = sessionControl->QueryInterface(IID_PPV_ARGS(&audioVolume));
				NOTIFY_IF_FAILED(L"IAudioSessionControl::QueryInterface failed", hr, return false);

				if (!foundStream)
				{
					hr = audioVolume->GetMute(&mute);
					NOTIFY_IF_FAILED(L"GetMute failed", hr, return false);

					foundStream = true;
					mute = !mute;
				}

				hr = audioVolume->SetMute(mute, nullptr);
				NOTIFY_IF_FAILED(L"SetMute failed", hr, return false);
			}
		}

		c16* muteStr = foundStream
			? mute ? L"Muted" : L"Unmuted"
			: L"No Audio";

		c16 message[256];
		swprintf(message, ArrayCount(message), L"%s - %s", muteStr, focusedProcess.name);
		Notify(notification, message);
	}

	return true;

	#undef NOTIFY_IF
	#undef NOTIFY_IF_FAILED
	#undef NOTIFY_IF_INVALID_HANDLE
}

b32
OpenVolumeMixerWindow(NotificationWindow* notification)
{
	// Command line options (Windows 10 22H2)
	//
	// sndvol <options> <coordinates>
	//
	// -f Volume control (single master slider)
	// -m Unknown, used by systray, probably "mixer"
	// -p Select which devices have a separate slider
	// -s Unknown, doesn't show up
	//
	// Coordinates y * 65536 + x

	v2i GetCurrentResolution();
	v2i resolution = GetCurrentResolution();

	// Mostly just copying what Windows does by default. It passes coordinates that are offscreen and
	// then either Windows or SndVol clamps the window to keep it visible. Windows actually uses a
	// 10/11 pixel offset, but the net result is exactly the same.
	i32 coords = resolution.y * 65536 + resolution.x;

	c8 command[256];
	snprintf(command, ArrayCount(command), "SndVol.exe -m %i", coords);
	return RunCommand(notification, command);
}
