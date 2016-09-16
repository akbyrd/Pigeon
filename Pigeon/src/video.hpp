#pragma once

inline bool
AreDisplayModesEqualIgnoringFrequency(DEVMODE* lhs, DEVMODE* rhs)
{
	return lhs->dmPelsWidth          == rhs->dmPelsWidth
		&& lhs->dmPelsHeight         == rhs->dmPelsHeight
		&& lhs->dmBitsPerPel         == rhs->dmBitsPerPel
		&& lhs->dmDisplayFlags       == rhs->dmDisplayFlags
		&& lhs->dmPosition.x         == rhs->dmPosition.x
		&& lhs->dmPosition.y         == rhs->dmPosition.y
		&& lhs->dmDisplayOrientation == rhs->dmDisplayOrientation;
}

//NOTE: CoInitialize is assumed to have been called.
inline bool
CycleRefreshRate()
{
	//Get current display settings
	DEVMODEW currentDisplaySettings = {};
	{
		currentDisplaySettings.dmSize = sizeof(currentDisplaySettings);

		b32 success = EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &currentDisplaySettings);
		if (!success) return false;
	}


	//Find the next available display settings
	DEVMODEW newDisplaySettings = {};
	{
		newDisplaySettings.dmSize = sizeof(newDisplaySettings);

		DEVMODEW displaySettings = {};
		displaySettings.dmSize = sizeof(newDisplaySettings);

		bool foundDesiredFrequency = false;

		//TODO: Check all failure modes
		int i = 0;
		while (EnumDisplaySettingsExW(nullptr, i++, &displaySettings, EDS_RAWMODE))
		{
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
					//Set desired frequency again next time around.
					foundDesiredFrequency = false;
				}
			}
		}

		//This can occur e.g. when the display has only one desirable frequency.
		if (!foundDesiredFrequency) return false;
	}


	//Set the next display settings
	{
		newDisplaySettings.dmFields = DM_DISPLAYFREQUENCY;

		LONG result = ChangeDisplaySettingsW(&newDisplaySettings, CDS_UPDATEREGISTRY | CDS_GLOBAL);
		if (result < 0) return false;
	}

	return true;
}

inline bool
OpenDisplayAdapterSettingsWindow()
{
	c8 commandLine[MAX_PATH + 256] = "\"";
	u16 endIndex = GetSystemDirectoryA(commandLine+1, ArrayCount(commandLine)-1);
	if (endIndex == 0 || ++endIndex > ArrayCount(commandLine)) return false;

	//NOTE: Path does not end with a backslash unless the
	//system directory is the root directory
	if (commandLine[endIndex-1] != '\\')
		commandLine[endIndex++] = '\\';

	c8 adapterSettingsCommand[] = "rundll32.exe\" display.dll,ShowAdapterSettings";
	u16 totalSize = endIndex + ArrayCount(adapterSettingsCommand);
	if (totalSize > ArrayCount(commandLine)) return false;

	for (u16 i = 0; i < ArrayCount(adapterSettingsCommand); ++i)
		commandLine[endIndex++] = adapterSettingsCommand[i];

	u32 result = WinExec(commandLine, SW_NORMAL);
	if (result < 32) return false;

	return true;
}