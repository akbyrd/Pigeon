inline b32
AreDisplayModesEqualIgnoringFrequency(DEVMODE* lhs, DEVMODE* rhs)
{
	/* NOTE: Only dmBitsPerPel, dmPelsWidth, dmPelsHeight, dmDisplayFlags,
	 * and dmDisplayFrequency are set by EnumDisplaySettings
	 */
	return lhs->dmPelsWidth    == rhs->dmPelsWidth
	    && lhs->dmPelsHeight   == rhs->dmPelsHeight
	    && lhs->dmBitsPerPel   == rhs->dmBitsPerPel
	    && lhs->dmDisplayFlags == rhs->dmDisplayFlags;
}

// NOTE: CoInitialize is assumed to have been called.
inline b32
CycleRefreshRate(NotificationWindow* notification)
{
	// Get current display settings
	DEVMODEW currentDisplaySettings = {};
	{
		currentDisplaySettings.dmSize = sizeof(currentDisplaySettings);

		// NOTE: This and the Ex variant only fail when iModeNum is out of range
		EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &currentDisplaySettings);
	}


	// Find the next available display settings
	DEVMODEW newDisplaySettings = {};
	{
		newDisplaySettings.dmSize = sizeof(newDisplaySettings);

		DEVMODEW displaySettings = {};
		displaySettings.dmSize = sizeof(displaySettings);

		bool foundDesiredFrequency = false;

		// NOTE: The OS caches display information when i == 0. Other values use the cache.
		int i = 0;
		while (EnumDisplaySettingsExW(nullptr, i++, &displaySettings, EDS_RAWMODE))
		{
			u32 requiredFlags = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_DISPLAYFREQUENCY;
			if (displaySettings.dmFields & requiredFlags != requiredFlags)
			{
				Notify(notification, L"EnumDisplaySettingsEx didn't set necessary fields", Error::Warning);
				continue;
			}

			if (AreDisplayModesEqualIgnoringFrequency(&displaySettings, &currentDisplaySettings))
			{
				if (displaySettings.dmDisplayFrequency < 60) continue;

				if (!foundDesiredFrequency)
				{
					foundDesiredFrequency = true;
					newDisplaySettings.dmDisplayFrequency = displaySettings.dmDisplayFrequency;
				}

				if (displaySettings.dmDisplayFrequency == currentDisplaySettings.dmDisplayFrequency)
				{
					// Set desired frequency again next time around.
					foundDesiredFrequency = false;
				}
			}
		}

		// This can occur e.g. when the display has only one desirable frequency.
		if (!foundDesiredFrequency) return false;
	}


	// Set the next display settings
	{
		newDisplaySettings.dmFields = DM_DISPLAYFREQUENCY;

		i32 iResult = ChangeDisplaySettingsW(&newDisplaySettings, CDS_UPDATEREGISTRY | CDS_GLOBAL);
		if (iResult != DISP_CHANGE_SUCCESSFUL)
		{
			NotifyFormat(notification, L"ChangeDisplaySettings failed: %i", Error::Warning, iResult);
			return false;
		}

		NotifyFormat(notification, L"%u Hz", newDisplaySettings.dmDisplayFrequency);
	}

	return true;
}

inline b32
OpenDisplayAdapterSettingsWindow(NotificationWindow* notification)
{
	/*  NOTE: The path to rundll32 can't exceed MAX_PATH, we we can never
	 * overflow the buffer as long as the options being passed in are under the
	 * extra 256 being allocated so I'm not going to bother checking after
	 * every operation.
	 */
	const u16 commandMaxLength = MAX_PATH + 256;

	c8 commandLine[commandMaxLength] = "\"";
	u16 commandLength = 1;

	u16 systemDirCount = GetSystemDirectoryA(commandLine+commandLength, commandMaxLength-commandLength);
	if (systemDirCount == 0)
	{
		NotifyWindowsError(notification, L"GetSystemDirectory failed");
		return false;
	}
	commandLength += systemDirCount;

	/* NOTE: Path does not end with a backslash unless the system directory is
	 * the root directory
	 */
	if (commandLine[commandLength-1] != '\\')
		commandLine[commandLength++] = '\\';

	c8 adapterSettingsCommand[] = "rundll32.exe\" display.dll,ShowAdapterSettings";
	for (u16 i = 0; i < ArrayCount(adapterSettingsCommand); ++i)
		commandLine[commandLength++] = adapterSettingsCommand[i];

	u32 uResult = WinExec(commandLine, SW_NORMAL);
	if (uResult < 32)
	{
		NotifyFormat(notification, L"GetSystemDirectory failed: %u", Error::Warning, uResult);
		return false;
	}

	return true;
}