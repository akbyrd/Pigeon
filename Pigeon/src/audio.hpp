#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <playsoundapi.h>
#include <propkey.h>
#include <tlhelp32.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "IPolicyConfig.h"

#define WM_AUDIO_DEVICE_CHANGED WM_USER + 1
#define WM_AUDIO_DEVICE_CONNECTED WM_USER + 2

enum struct AudioType
{
	Null,
	Playback,
	Recording,
};

b32
CycleAudioDevice(NotificationState* state, AudioType audioType)
{
	// NOTE: CoInitialize is assumed to have been called.

	// Shared
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	WARN_IF_FAILED(hr, return false, L"CoCreateInstance failed");

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
		WARN_IF_FAILED(hr, return false, L"EnumAudioEndpoints failed");

		hr = deviceCollection->GetCount(&deviceCount);
		WARN_IF_FAILED(hr, return false, L"GetCount failed");

		WARN_IF(deviceCount == 0, return true, L"No devices found")
	}


	// Get current audio device
	CComHeapPtr<c16> currentDefaultDeviceID;
	{
		CComPtr<IMMDevice> currentDevice;
		hr = deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, ERole::eConsole, &currentDevice);
		WARN_IF_FAILED(hr, return false, L"GetDefaultAudioEndpoint failed");

		hr = currentDevice->GetId(&currentDefaultDeviceID);
		WARN_IF_FAILED(hr, return false, L"GetId failed");
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
			WARN_IF_FAILED(hr, continue, L"Item failed");

			CComHeapPtr<c16> deviceID;
			hr = device->GetId(&deviceID);
			WARN_IF_FAILED(hr, continue, L"GetId failed");

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


	// Set next audio device
	{
		CComPtr<IPolicyConfig> policyConfig;
		hr = CoCreateInstance(CLSID_CPolicyConfigClient, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policyConfig));
		WARN_IF_FAILED(hr, return false, L"CoCreateInstance failed");

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eConsole);
		WARN_IF_FAILED(hr, return false, L"SetDefaultEndpoint failed");

		// NOTE: Console and multimedia are tied together. Setting both leads to "duplicate" notifications
		//hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eMultimedia);
		//WARN_IF_FAILED(hr, return false, L"SetDefaultEndpoint failed");

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eCommunications);
		WARN_IF_FAILED(hr, return false, L"SetDefaultEndpoint failed");


		if (audioType == AudioType::Playback)
		{
			b32 success = PlaySoundW((c16*) SND_ALIAS_SYSTEMDEFAULT, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SYSTEM);
			WARN_IF(!success, return false, L"PlaySound failed");
		}
	}

	return true;
}

b32
CycleAudioPlaybackDevice(NotificationState* state)
{
	// NOTE: CoInitialize is assumed to have been called.
	return CycleAudioDevice(state, AudioType::Playback);
}

b32
OpenAudioPlaybackDevicesWindow(NotificationState* state)
{
	c8 command[] = "control.exe /name Microsoft.Sound /page Playback";
	return RunCommand(state, command);
}

b32
CycleAudioRecordingDevice(NotificationState* state)
{
	// NOTE: CoInitialize is assumed to have been called.
	return CycleAudioDevice(state, AudioType::Recording);
}

b32
OpenAudioRecordingDevicesWindow(NotificationState* state)
{
	c8 command[] = "control.exe /name Microsoft.Sound /page Recording";
	return RunCommand(state, command);
}

b32
GetDeviceName(NotificationState* state, CComPtr<IMMDevice>& device, PROPVARIANT& deviceDescription)
{
	const wchar_t* genericNames[] = {
		L"Speaker",
		L"Headset",
		L"Headset Earphone",
		L"Headphones",
		L"Microphone",
	};

	CComPtr<IPropertyStore> propertyStore;
	HRESULT hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
	WARN_IF_FAILED(hr, return false, L"OpenPropertyStore failed");

	PropVariantInit(&deviceDescription);

	hr = propertyStore->GetValue(PKEY_Device_DeviceDesc, &deviceDescription);
	WARN_IF_FAILED(hr, return false, L"GetValue failed");

	for (const c16* genericName : genericNames)
	{
		if (wcscmp(genericName, deviceDescription.pwszVal) == 0)
		{
			hr = PropVariantClear(&deviceDescription);
			WARN_IF_FAILED(hr, return false, L"PropVariantClear failed");

			PropVariantInit(&deviceDescription);

			hr = propertyStore->GetValue(PKEY_DeviceInterface_FriendlyName, &deviceDescription);
			WARN_IF_FAILED(hr, return false, L"GetValue failed");

			break;
		}
	}

	return true;
}

b32
ReleaseDeviceName(NotificationState* state, PROPVARIANT& deviceDescription)
{
	HRESULT hr = PropVariantClear(&deviceDescription);
	WARN_IF_FAILED(hr, return false, L"PropVariantClear failed");

	return true;
}

void
ShowAudioDeviceConnected(NotificationState* state, c16* deviceID)
{
	defer { delete[] deviceID; };

	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	WARN_IF_FAILED(hr, return, L"CoCreateInstance failed");

	CComPtr<IMMDevice> device;
	hr = deviceEnumerator->GetDevice(deviceID, &device);
	WARN_IF_FAILED(hr, return, L"GetDevice failed");

	PROPVARIANT deviceDescription;
	b32 success = GetDeviceName(state, device, deviceDescription);
	if (!success) return;

	c16 message[256];
	swprintf(message, ArrayCount(message), L"%s\nConnected", deviceDescription.pwszVal);
	Notify(state, Severity::Info, message);

	success = ReleaseDeviceName(state, deviceDescription);
	if (!success) return;
}

void
ShowAudioDeviceDefaultChanged(NotificationState* state, AudioType audioType)
{
	EDataFlow dataFlow = (EDataFlow) -1;
	switch (audioType)
	{
		default: Assert(false); break;
		case AudioType::Playback:  dataFlow = EDataFlow::eRender;  break;
		case AudioType::Recording: dataFlow = EDataFlow::eCapture; break;
	}

	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	WARN_IF_FAILED(hr, return, L"CoCreateInstance failed");

	CComPtr<IMMDevice> device;
	hr = deviceEnumerator->GetDefaultAudioEndpoint(dataFlow, ERole::eConsole, &device);
	WARN_IF_FAILED(hr, return, L"GetDefaultAudioEndpoint failed");

	PROPVARIANT deviceDescription;
	b32 success = GetDeviceName(state, device, deviceDescription);
	if (!success) return;

	Notify(state, Severity::Info, deviceDescription.pwszVal);

	success = ReleaseDeviceName(state, deviceDescription);
	if (!success) return;
}

struct ProcessInfo
{
	u32 id;
	u32 rootId;
	c16 name[256];
};

void
GetProcessName(NotificationState* state, HANDLE snapshot, ProcessInfo& process)
{
	// NOTE: CoInitialize is assumed to have been called.

	// Open process returns 0, not INVALID_HANDLE_VALUE
	HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, false, process.id);
	WARN_IF(!processHandle, return, L"OpenProcess failed")
	defer { CloseHandle(processHandle); };

	c16 processPath[MAX_PATH];
	u32 pathLength = ArrayCount(processPath);
	b32 result = QueryFullProcessImageNameW(processHandle, 0, processPath, (DWORD*) &pathLength);
	WARN_IF(!result, return, L"QueryFullProcessImageNameW failed");


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
			wcscpy_s(process.name, tempDescription);
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
		WARN_IF(!result, break, L"GetFileVersionInfoSizeW failed");

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
					wcscpy_s(process.name, tempDescription);
					return;
				}
			}
		}
	}


	// If we didn't find a file description (UWP applications) use the display name from the shell
	{
		CComPtr<IShellItem2> shellItem;
		HRESULT hr = SHCreateItemFromParsingName(processPath, nullptr, IID_PPV_ARGS(&shellItem));
		WARN_IF_FAILED(hr, return, L"SHCreateItemFromParsingName failed");

		CComHeapPtr<c16> value;
		hr = shellItem->GetString(PKEY_ItemNameDisplayWithoutExtension, &value);
		WARN_IF_FAILED(hr, return, L"IShellItem2::GetString failed");

		wcscpy_s(process.name, value);
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
				wcscpy_s(process.name, processEntry.szExeFile);
				return;
			}
		}
	}


	// Failure
	{
		swprintf(process.name, ArrayCount(process.name), L"%i", process.id);
		return;
	}
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
ToggleMuteForCurrentApplication(NotificationState* state)
{
	// NOTE: CoInitialize is assumed to have been called.

	HANDLE snapshot = INVALID_HANDLE_VALUE;
	defer { CloseHandle(snapshot); };
	{
		snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		WARN_IF_INVALID_HANDLE(snapshot, return false, L"CreateToolhelp32Snapshot failed");
	}


	// Find the currently focused process
	ProcessInfo focusedProcess = {};
	{
		// GetForegroundWindow does not work for UWP applications.
		GUITHREADINFO threadInfo = {};
		threadInfo.cbSize = sizeof(GUITHREADINFO);
		b32 result = GetGUIThreadInfo(0, &threadInfo);
		WARN_IF(!result, return false, L"GetGUIThreadInfo failed");

		HWND hwnd = threadInfo.hwndFocus ? threadInfo.hwndFocus : threadInfo.hwndActive;
		WARN_IF(!hwnd, return false, L"Failed to find focused window");
		HRESULT hr = GetWindowThreadProcessId(hwnd, (DWORD*) &focusedProcess.id);
		WARN_IF_FAILED(hr, return false, L"GetWindowThreadProcessId failed");

		GetProcessName(state, snapshot, focusedProcess);
		GetProcessRoot(snapshot, focusedProcess);
	}


	// Mute all audio streams associated with the process
	{
		CComPtr<IMMDeviceEnumerator> deviceEnumerator;
		HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
		WARN_IF_FAILED(hr, return false, L"CoCreateInstance failed");

		CComPtr<IMMDevice> currentDevice;
		hr = deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &currentDevice);
		WARN_IF_FAILED(hr, return false, L"GetDefaultAudioEndpoint failed");

		CComPtr<IAudioSessionManager2> sessionManager;
		hr = currentDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, (void**) &sessionManager);
		WARN_IF_FAILED(hr, return false, L"IMMDevice::Activate failed");

		CComPtr<IAudioSessionEnumerator> sessionList;
		hr = sessionManager->GetSessionEnumerator(&sessionList);
		WARN_IF_FAILED(hr, return false, L"GetSessionEnumerator failed");

		int sessionCount;
		hr = sessionList->GetCount(&sessionCount);
		WARN_IF_FAILED(hr, return false, L"IAudioSessionEnumerator::GetCount failed");

		b8 foundStream = false;
		b32 mute = true;

		for (int i = 0; i < sessionCount; i++)
		{
			CComPtr<IAudioSessionControl> sessionControl;
			hr = sessionList->GetSession(i, &sessionControl);
			WARN_IF_FAILED(hr, return false, L"IAudioSessionEnumerator::GetSession failed");

			CComPtr<IAudioSessionControl2> sessionControl2;
			hr = sessionControl->QueryInterface(IID_PPV_ARGS(&sessionControl2));
			WARN_IF_FAILED(hr, return false, L"IAudioSessionControl::QueryInterface failed");

			ProcessInfo process = {};
			hr = sessionControl2->GetProcessId((DWORD*) &process.id);
			WARN_IF_FAILED(hr, return false, L"IAudioSessionControl2::GetProcessId failed");

			// TODO: Can this be done the other way around? Ask if stream process has a focused window

			// We use the root process because some applications
			// * Have a root process with an audio stream (Discord)
			// * Have audio streams and no windows (Discord, NVidia Container)
			GetProcessRoot(snapshot, process);
			if (process.rootId == focusedProcess.rootId)
			{
				CComPtr<ISimpleAudioVolume> audioVolume;
				hr = sessionControl->QueryInterface(IID_PPV_ARGS(&audioVolume));
				WARN_IF_FAILED(hr, return false, L"IAudioSessionControl::QueryInterface failed");

				if (!foundStream)
				{
					hr = audioVolume->GetMute(&mute);
					WARN_IF_FAILED(hr, return false, L"GetMute failed");

					foundStream = true;
					mute = !mute;
				}

				hr = audioVolume->SetMute(mute, nullptr);
				WARN_IF_FAILED(hr, return false, L"SetMute failed");
			}
		}

		c16* muteStr = foundStream
			? mute ? L"Muted" : L"Unmuted"
			: L"No Audio";

		c16 message[256];
		swprintf(message, ArrayCount(message), L"%s - %s", muteStr, focusedProcess.name);
		Notify(state, Severity::Info, message);
	}

	return true;
}

b32
OpenVolumeMixerWindow(NotificationState* state)
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
	return RunCommand(state, command);
}

struct AudioNotificationClient : public IMMNotificationClient
{
	u32 refs = 1;
	NotificationState* state;

	AudioNotificationClient(NotificationState* state) : state(state) {}

	ULONG AddRef() { return InterlockedIncrement(&refs); }

	ULONG Release()
	{
		u32 refs2 = InterlockedDecrement(&refs);
		if (refs2 == 0) delete this;
		return refs2;
	}

	HRESULT QueryInterface(REFIID riid, void** ppv)
	{
		if (!ppv) return E_INVALIDARG;

		if (riid == __uuidof(IUnknown) || riid == __uuidof(IMMNotificationClient))
		{
			*ppv = this;
			AddRef();
			return S_OK;
		}

		ppv = nullptr;
		return E_NOINTERFACE;
	}

	HRESULT OnDeviceStateChanged(LPCWSTR deviceID, DWORD dwNewState) override
	{
		switch (dwNewState)
		{
			case DEVICE_STATE_ACTIVE:
			{
				i32 len = lstrlenW(deviceID) + 1;
				c16* deviceIDCopy = new c16[len];
				wcscpy_s(deviceIDCopy, len, deviceID);

				PostMessageW(state->hwnd, WM_AUDIO_DEVICE_CONNECTED, 0, (LPARAM) deviceIDCopy);
				break;
			}

			case DEVICE_STATE_DISABLED:
			case DEVICE_STATE_NOTPRESENT:
			case DEVICE_STATE_UNPLUGGED:
				break;
		}
		return S_OK;
	}

	HRESULT OnDeviceAdded(LPCWSTR deviceID) override { return S_OK; }
	HRESULT OnDeviceRemoved(LPCWSTR deviceID) override { return S_OK; }

	HRESULT OnDefaultDeviceChanged(EDataFlow dataFlow, ERole role, LPCWSTR deviceID) override
	{
		// NOTE: This is called from another thread (but not during the message loop)
		switch (dataFlow)
		{
			case EDataFlow::eRender:
			{
				PostMessageW(state->hwnd, WM_AUDIO_DEVICE_CHANGED, 0, (LPARAM) AudioType::Playback);
				return S_OK;
			}

			case EDataFlow::eCapture:
			{
				PostMessageW(state->hwnd, WM_AUDIO_DEVICE_CHANGED, 0, (LPARAM) AudioType::Recording);
				return S_OK;
			}

			case EDataFlow::eAll:
			case EDataFlow::EDataFlow_enum_count:
				break;
		}

		WARN_IF(true, IGNORE, L"Unrecognized EDataFlow %d", dataFlow);
		return S_OK;
	}

	HRESULT OnPropertyValueChanged(LPCWSTR deviceID, const PROPERTYKEY key) override { return S_OK; }
};
