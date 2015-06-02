namespace GameSettings
{
	using System.Collections.Generic;

	internal partial class DisplayController
	{
		private User32.DEVMODE currentDisplaySettings;

		public DisplayController ()
		{
			currentDisplaySettings = new User32.DEVMODE();
			//TODO: Set size
			//currentDisplaySettings.dmSize = sizeof(User32.DEVMODE);
			User32.EnumDisplaySettings(null, User32.ENUM_CURRENT_SETTINGS, ref currentDisplaySettings);
		}

		#region Public Interface

		/// <summary>
		/// Get the supported refresh rates at the current resolution.
		/// </summary>
		public IList<int> GetSupportedRefreshRates ()
		{
			List<int> supportedRefreshRates = new List<int>();
			User32.DEVMODE devmode = new User32.DEVMODE();

			int i = 0;
			while ( User32.EnumDisplaySettings(null, i++, ref devmode) )
			{
				if ( devmode.dmPelsWidth  == currentDisplaySettings.dmPelsWidth
				  && devmode.dmPelsHeight == currentDisplaySettings.dmPelsHeight )
				{
					supportedRefreshRates.Add(devmode.dmDisplayFrequency);
				}
			}

			//Sort so we can cycle through in a sensible manner
			supportedRefreshRates.Sort();

			//Remove duplicates
			for ( int j = supportedRefreshRates.Count-2; j >= 0; --j )
				if ( supportedRefreshRates[j] == supportedRefreshRates[j+1] )
					supportedRefreshRates.RemoveAt(j);

			return supportedRefreshRates;
		}

		public void SetRefreshRate ( int refreshRate )
		{
			if ( currentDisplaySettings.dmDisplayFrequency == refreshRate ) { return; }

			User32.DEVMODE devmode = currentDisplaySettings;
			devmode.dmDisplayFrequency = refreshRate;

			User32.DISP_CHANGE result = User32.ChangeDisplaySettings(ref devmode, User32.ChangeDisplaySettingsFlags.CDS_NONE);

			if ( result == User32.DISP_CHANGE.Successful )
				currentDisplaySettings = devmode;
		}

		public void ToggleRefreshRate ()
		{
			IList<int> supportedRefreshRates = GetSupportedRefreshRates();

			//TODO: Dialog box?
			if ( supportedRefreshRates.Count == 0 ) { return; }

			//Determine the index of the current refresh rate so we can increment it
			int currentRefreshRateIndex = supportedRefreshRates.IndexOf(currentDisplaySettings.dmDisplayFrequency);

			//Increment the current refresh rate
			currentRefreshRateIndex = (currentRefreshRateIndex + 1) % supportedRefreshRates.Count;

			SetRefreshRate(supportedRefreshRates[currentRefreshRateIndex]);
		}

		#endregion

		#region Utility Methods

		private static Resolution GetCurrentResolution ()
		{
			//Get current display settings
			User32.DEVMODE devmode = new User32.DEVMODE();
			User32.EnumDisplaySettings(null, User32.ENUM_CURRENT_SETTINGS, ref devmode);

			Resolution currentResolution = DevmodeToResolution(devmode);
			return currentResolution;
		}

		private static Resolution DevmodeToResolution ( User32.DEVMODE devmode )
		{
			Resolution res = new Resolution(devmode.dmPelsWidth, devmode.dmPelsHeight, devmode.dmDisplayFrequency);
			return res;
		}

		#endregion
	}
}