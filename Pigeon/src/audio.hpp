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


	#define NOTIFY_IF(expression, string, reaction)      \
	if (expression)                                      \
	{                                                    \
		Notify(notification, string, Severity::Warning); \
		reaction;                                        \
	}                                                    \

	#define NOTIFY_IF_FAILED(string, hr, reaction)                       \
	if (FAILED(hr))                                                      \
	{                                                                    \
		NotifyWindowsError(notification, string, Severity::Warning, hr); \
		reaction;                                                        \
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

	c8 command[1024];
	snprintf(command, ArrayCount(command), "SndVol.exe -m %i", coords);
	return RunCommand(notification, command);
}
