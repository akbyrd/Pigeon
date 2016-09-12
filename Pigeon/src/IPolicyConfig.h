//Source: http://www.daveamenta.com/2011-05/programmatically-or-command-line-change-the-default-sound-playback-device-in-windows-7/

#pragma once

  interface DECLSPEC_UUID("f8679f50-850a-41cf-9c72-430f290290c8") IPolicyConfig; //Win10 1607
//interface DECLSPEC_UUID("6be54be8-a068-4875-a49d-0c2966473b11") IPolicyConfig; //Win10 1511
//interface DECLSPEC_UUID("ca286fc3-91fd-42c3-8e9b-caafa66242e3") IPolicyConfig; //Win10 1507
//interface DECLSPEC_UUID("8f9fb2aa-1c0b-4d54-b6bb-b2f2a10ce03c") IPolicyConfig; //Win10?
//interface DECLSPEC_UUID("00000000-0000-0000-C000-000000000046") IPolicyConfig; //IUnknown
const IID IID_IPolicyConfig = __uuidof(IPolicyConfig);

class DECLSPEC_UUID("870af99c-171d-4f9e-af0d-e63df40c2bc9") CPolicyConfigClient; //Win7+
const CLSID CLSID_CPolicyConfigClient = __uuidof(CPolicyConfigClient);

interface IPolicyConfig : public IUnknown
{
public:

	virtual HRESULT GetMixFormat(PCWSTR deviceName, WAVEFORMATEX** ppFormat);

	virtual HRESULT STDMETHODCALLTYPE GetDeviceFormat(PCWSTR deviceName, INT bDefault, WAVEFORMATEX** ppFormat);
	virtual HRESULT STDMETHODCALLTYPE SetDeviceFormat(PCWSTR deviceName, WAVEFORMATEX* pEndpointFOrmat, WAVEFORMATEX* pMixFormat);
	virtual HRESULT STDMETHODCALLTYPE ResetDeviceFormat(PCWSTR deviceName);

	virtual HRESULT STDMETHODCALLTYPE GetProcessingPeriod(PCWSTR deviceName, INT bDefault, PINT64 pmftDefaultPeriod, PINT64 pmftMinimumPeriod);
	virtual HRESULT STDMETHODCALLTYPE SetProcessingPeriod(PCWSTR deviceName, PINT64 pmftPeriod);

	virtual HRESULT STDMETHODCALLTYPE GetShareMode(PCWSTR deviceName, struct DeviceShareMode* pMode);
	virtual HRESULT STDMETHODCALLTYPE SetShareMode(PCWSTR deviceName, struct DeviceShareMode* pMode);

	virtual HRESULT STDMETHODCALLTYPE GetPropertyValue(PCWSTR deviceName, const PROPERTYKEY& bFXStore, PROPVARIANT* pv);
	virtual HRESULT STDMETHODCALLTYPE SetPropertyValue(PCWSTR deviceName, const PROPERTYKEY& bFXStore, PROPVARIANT* pv);

	virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(__in PCWSTR wszDeviceId, __in ERole eRole);

	virtual HRESULT STDMETHODCALLTYPE SetEndpointVisibility(PCWSTR deviceName, INT);
};