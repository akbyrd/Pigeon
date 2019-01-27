#include <mmdeviceapi.h>
#include <playsoundapi.h>
#include <Functiondiscoverykeys_devpkey.h>

#include "IPolicyConfig.h"

enum struct AudioType
{
	Null,
	Playback,
	Recording,
};

static b32
CycleAudioDevice(NotificationWindow* notification, AudioType audioType)
{
	// NOTE: CoInitialize is assumed to have been called.
	HRESULT hr;


	#define NOTIFY_IF_FAILED(string, hr, reaction)                       \
	if (FAILED(hr))                                                      \
	{                                                                    \
		NotifyWindowsError(notification, string, Severity::Warning, hr); \
		reaction;                                                        \
	}                                                                    \


	// Shared
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
	NOTIFY_IF_FAILED(L"CoCreateInstance failed", hr, return false);

	EDataFlow dataFlow;
	switch (audioType)
	{
		default: Assert(false); break;
		case AudioType::Playback:  dataFlow = EDataFlow::eRender;  break;
		case AudioType::Recording: dataFlow = EDataFlow::eCapture; break;
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
	CComPtr<IMMDeviceCollection> deviceCollection;
	{
		hr = deviceEnumerator->EnumAudioEndpoints(dataFlow, DEVICE_STATE_ACTIVE, &deviceCollection);
		NOTIFY_IF_FAILED(L"EnumAudioEndpoints failed", hr, return false);

		u32 deviceCount;
		hr = deviceCollection->GetCount(&deviceCount);
		NOTIFY_IF_FAILED(L"GetCount failed", hr, return false);

		bool useNextDevice = false;
		for (u32 i = 0; i < deviceCount; ++i)
		{
			CComPtr<IMMDevice> device;
			hr = deviceCollection->Item(i, &device);
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
				continue;
			}

			if (!newDefaultDeviceID)
			{
				newDefaultDeviceID = deviceID;
			}
		}
	}


	// Notification
	{
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
	c8 command[] = "control.exe\" /name Microsoft.Sound /page Playback";
	return RunCommand(notification, command, ArrayCount(command));
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
	c8 command[] = "control.exe\" /name Microsoft.Sound /page Recording";
	return RunCommand(notification, command, ArrayCount(command));
}
