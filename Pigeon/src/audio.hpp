#include <mmdeviceapi.h>
#include <playsoundapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "IPolicyConfig.h"

inline b32
InitializeAudio(NotificationWindow* notification)
{
	HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_SPEED_OVER_MEMORY | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
	{
		NotifyWindowsError(notification, L"CoInitializeEx failed", Error::Error, hr);
		return false;
	}

	return true;
}

inline b32
CycleDefaultAudioDevice(NotificationWindow* notification)
{
	HRESULT hr;


	//Shared
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	if (FAILED(hr)) return false;


	//Get current audio device
	CComHeapPtr<c16> currentDefaultDeviceID;
	{
		CComPtr<IMMDevice> currentPlaybackDevice;
		hr = deviceEnumerator->GetDefaultAudioEndpoint(EDataFlow::eRender, ERole::eConsole, &currentPlaybackDevice);
		if (FAILED(hr)) return false;

		hr = currentPlaybackDevice->GetId(&currentDefaultDeviceID);
		if (FAILED(hr)) return false;
	}


	//Find next available audio device
	CComHeapPtr<c16> newDefaultDeviceID;
	CComPtr<IMMDeviceCollection> deviceCollection;
	{
		hr = deviceEnumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &deviceCollection);
		if (FAILED(hr)) return false;

		u32 deviceCount;
		hr = deviceCollection->GetCount(&deviceCount);
		if (FAILED(hr)) return false;

		bool useNextDevice = false;
		for (u32 i = 0; i < deviceCount; ++i)
		{
			CComPtr<IMMDevice> device;
			hr = deviceCollection->Item(i, &device);
			if (FAILED(hr)) continue;

			CComHeapPtr<c16> deviceID;
			hr = device->GetId(&deviceID);
			if (FAILED(hr)) continue;

			if (useNextDevice)
			{
				newDefaultDeviceID = deviceID;
				break;
			}

			if (wcscmp(deviceID, currentDefaultDeviceID) == 0)
			{
				useNextDevice = true;
				continue;
			}

			if (!newDefaultDeviceID)
			{
				newDefaultDeviceID = deviceID;
			}
		}
	}


	//Notification
	{
		CComPtr<IMMDevice> device;
		hr = deviceEnumerator->GetDevice(newDefaultDeviceID, &device);
		if (FAILED(hr)) return false;

		CComPtr<IPropertyStore> propertyStore;
		hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
		if (FAILED(hr)) return false;

		PROPVARIANT deviceDescription;
		PropVariantInit(&deviceDescription);

		hr = propertyStore->GetValue(PKEY_Device_DeviceDesc, &deviceDescription);
		if (FAILED(hr)) return false;

		// TODO: I think this will end up reading from freed memory if
		// the notification is queued. However, normal (non-warning/error)
		// notifications are currently never queued.
		Notify(notification, deviceDescription.pwszVal);

		hr = PropVariantClear(&deviceDescription);
		if (FAILED(hr)) return false;
	}


	//Set next audio device
	{
		CComPtr<IPolicyConfig> policyConfig;
		hr = CoCreateInstance(CLSID_CPolicyConfigClient, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policyConfig));
		if (FAILED(hr)) return false;

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eConsole);
		if (FAILED(hr)) return false;

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eMultimedia);
		if (FAILED(hr)) return false;

		b32 success = PlaySoundW((c16*) SND_ALIAS_SYSTEMDEFAULT, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SYSTEM);
		if (!success) return false;
	}

	return true;
}

inline bool
OpenAudioPlaybackDevicesWindow()
{
	c8 commandLine[MAX_PATH + 256] = "\"";
	u16 endIndex = GetSystemDirectoryA(commandLine+1, ArrayCount(commandLine)-1);
	if (endIndex == 0 || ++endIndex > ArrayCount(commandLine)) return false;

	// NOTE: Path does not end with a backslash unless the
	// system directory is the root directory
	if (commandLine[endIndex-1] != '\\')
		commandLine[endIndex++] = '\\';

	c8 canonicalSoundPath[] = "control.exe\" /name Microsoft.Sound";
	u16 totalSize = endIndex + ArrayCount(canonicalSoundPath);
	if (totalSize > ArrayCount(commandLine)) return false;

	for (u16 i = 0; i < ArrayCount(canonicalSoundPath); ++i)
		commandLine[endIndex++] = canonicalSoundPath[i];

	u32 result = WinExec(commandLine, SW_NORMAL);
	if (result < 32) return false;

	return true;
}

inline void
TeardownAudio()
{
	// TODO: This can enter a modal loop and dispatch messages. Understand the implications of this.
	CoUninitialize();
}