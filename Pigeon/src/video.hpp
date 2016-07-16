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
	b32 success;


	//Get current display settings
	DEVMODEW currentDisplaySettings = {};
	{
		currentDisplaySettings.dmSize = sizeof(currentDisplaySettings);

		success = EnumDisplaySettingsW(nullptr, ENUM_CURRENT_SETTINGS, &currentDisplaySettings);
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
		LONG result;

		newDisplaySettings.dmFields = DM_DISPLAYFREQUENCY;

		result = ChangeDisplaySettingsW(&newDisplaySettings, CDS_UPDATEREGISTRY | CDS_GLOBAL);
		if (result < 0) return false;
	}

	return true;
}