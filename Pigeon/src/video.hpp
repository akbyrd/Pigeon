b32
AreDisplayModesEqualIgnoringFrequency(DEVMODE* lhs, DEVMODE* rhs)
{
	// NOTE: Only dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags, and dmDisplayFrequency are
	// set by EnumDisplaySettings
	return lhs->dmPelsWidth    == rhs->dmPelsWidth
	    && lhs->dmPelsHeight   == rhs->dmPelsHeight
	    && lhs->dmBitsPerPel   == rhs->dmBitsPerPel
	    && lhs->dmDisplayFlags == rhs->dmDisplayFlags;
}

template <typename T>
T
Abs(T value)
{
	return value >= 0 ? value : (T) -1 * value;
}

v2i GetCurrentResolution()
{
	DEVMODEW currentDisplaySettings = {};
	currentDisplaySettings.dmSize = sizeof(currentDisplaySettings);

	// NOTE: This and the Ex variant only fail when iModeNum is out of range
	EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &currentDisplaySettings);

	v2i result = {};
	result.x = currentDisplaySettings.dmPelsWidth;
	result.y = currentDisplaySettings.dmPelsHeight;
	return result;
}

// NOTE: CoInitialize is assumed to have been called.
b32
SetMaximumRefreshRate(NotificationState* state)
{
	// Get current display settings
	DEVMODEW currentDisplaySettings = {};
	{
		currentDisplaySettings.dmSize = sizeof(currentDisplaySettings);

		// NOTE: This and the Ex variant only fail when iModeNum is out of range
		EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &currentDisplaySettings);
	}


	// Find the highest available refresh
	DEVMODEW newDisplaySettings = {};
	{
		newDisplaySettings.dmSize = sizeof(newDisplaySettings);

		DEVMODEW displaySettings = {};
		displaySettings.dmSize = sizeof(displaySettings);

		// NOTE: The OS caches display information when i == 0. Other values use the cache.
		i32 i = 0;
		while (EnumDisplaySettingsExW(nullptr, i++, &displaySettings, EDS_RAWMODE))
		{
			u32 requiredFlags = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
			b8 success = (displaySettings.dmFields & requiredFlags) == requiredFlags;
			NOTIFY_IF(!success, Severity::Warning, continue, L"EnumDisplaySettingsEx didn't set necessary fields");

			if (!AreDisplayModesEqualIgnoringFrequency(&displaySettings, &currentDisplaySettings))
				continue;

			if (displaySettings.dmDisplayFrequency > newDisplaySettings.dmDisplayFrequency)
				newDisplaySettings.dmDisplayFrequency = displaySettings.dmDisplayFrequency;
		}
	}


	// Set the next display settings
	{
		newDisplaySettings.dmFields = DM_DISPLAYFREQUENCY;

		i32 iResult = ChangeDisplaySettingsW(&newDisplaySettings, CDS_UPDATEREGISTRY | CDS_GLOBAL);
		NOTIFY_IF(iResult != DISP_CHANGE_SUCCESSFUL, Severity::Warning, return false, L"ChangeDisplaySettings failed: %i", iResult);

		NotifyFormat(state, L"%u Hz", newDisplaySettings.dmDisplayFrequency);
	}

	return true;
}

b32
OpenDisplayAdapterSettingsWindow(NotificationState* state)
{
	c8 command[] = "rundll32.exe display.dll,ShowAdapterSettings";
	return RunCommand(state, command);
}
