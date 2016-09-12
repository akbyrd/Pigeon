#pragma once

#include <mmdeviceapi.h>
//#include <Functiondiscoverykeys_devpkey.h>
const IID    IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);

#include "IPolicyConfig.h"

//NOTE: CoInitialize is assumed to have been called.
inline bool
CycleDefaultAudioDevice()
{
	HRESULT hr;


	//Shared
	CComPtr<IMMDeviceEnumerator> deviceEnumerator;
	hr = CoCreateInstance(CLSID_MMDeviceEnumerator, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&deviceEnumerator));
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
	{
		CComPtr<IMMDeviceCollection> deviceCollection;
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

			#if false
			CComPtr<IPropertyStore> propertyStore;
			hr = device->OpenPropertyStore(STGM_READ, &propertyStore);
			if (FAILED(hr)) continue;

			PROPVARIANT friendlyName;
			PropVariantInit(&friendlyName);

			hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &friendlyName);
			if (FAILED(hr)) continue;

			DebugPrint(L"Device: %d: %ls\n", i, friendlyName.pwszVal);

			hr = PropVariantClear(&friendlyName);
			if (FAILED(hr)) continue;
			#endif

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


	//Set next audio device
	{
		b32 success;

		CComPtr<IPolicyConfig> policyConfig;
		hr = CoCreateInstance(CLSID_CPolicyConfigClient, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policyConfig));
		if (FAILED(hr)) return false;

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eConsole);
		if (FAILED(hr)) return false;

		hr = policyConfig->SetDefaultEndpoint(newDefaultDeviceID, ERole::eMultimedia);
		if (FAILED(hr)) return false;

		success = PlaySoundW((c16*) SND_ALIAS_SYSTEMDEFAULT, nullptr, SND_ALIAS_ID | SND_ASYNC | SND_SYSTEM);
		if (!success) return false;
	}

	return true;
}

void
OpenAudioPlaybackDevices()
{
	c8 commandLine[MAX_PATH + 256] = "\"";
	u16 endIndex = GetSystemDirectoryA(commandLine+1, ArrayCount(commandLine)-1);
	if (endIndex == 0 || ++endIndex > ArrayCount(commandLine)) return;

	//NOTE: Path does not end with a backslash unless the
	//system directory is the root directory
	if (commandLine[endIndex-1] != '\\')
		commandLine[endIndex++] = '\\';

	c8 canonicalSoundPath[] = "control.exe\" /name Microsoft.Sound";
	u16 totalSize = endIndex + ArrayCount(canonicalSoundPath);
	if (totalSize > ArrayCount(commandLine)) return;

	for (u16 i = 0; i < ArrayCount(canonicalSoundPath); ++i)
		commandLine[endIndex++] = canonicalSoundPath[i];

	u32 result = WinExec(commandLine, SW_NORMAL);
	if (result < 32) return;
}