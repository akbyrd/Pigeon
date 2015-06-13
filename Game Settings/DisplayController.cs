namespace GameSettings
{
	using System.Collections.Generic;
	using System.Linq;
	using System.Runtime.InteropServices;

	internal partial class DisplayController
	{
		private User32.DEVMODE currentDisplaySettings;

		public DisplayController ()
		{
			currentDisplaySettings = ConstructDEVMODE();
			User32.EnumDisplaySettings(null, User32.ENUM_CURRENT_SETTINGS, ref currentDisplaySettings);
		}

		#region Public Interface

		/// <summary>
		/// Get the supported refresh rates at the current resolution.
		/// </summary>
		public IList<int> GetSupportedRefreshRates ()
		{
			List<int> supportedRefreshRates = new List<int>();
			User32.DEVMODE devmode = ConstructDEVMODE();

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

		public Result SetRefreshRate ( int refreshRate )
		{
			User32.DISP_CHANGE dispResult = User32.DISP_CHANGE.Successful;
			User32.DEVMODE devmode = currentDisplaySettings;

			if ( currentDisplaySettings.dmDisplayFrequency != refreshRate )
			{
				devmode.dmDisplayFrequency = refreshRate;
				dispResult = User32.ChangeDisplaySettings(ref devmode, User32.ChangeDisplaySettingsFlags.CDS_NONE);
			}

			Result result;
			if ( dispResult == User32.DISP_CHANGE.Successful )
			{
				currentDisplaySettings = devmode;

				result = new Result(
					false,
					"Set refresh rate to " + refreshRate + " Hz",
					"Refresh Rate: " + refreshRate + " Hz",
					true
				);
			}
			else
			{
				result = new Result(
					true,
					"Error setting refresh rate to " + refreshRate + ": " + dispResult,
					"Refresh Rate Error: " + dispResult
				);
			}

			return result;
		}

		public Result ToggleRefreshRate ( IList<int> refreshRates = null )
		{
			if ( refreshRates == null )
			{
				refreshRates = GetSupportedRefreshRates();

				if ( refreshRates.Count == 0 )
				{
					return new Result(
						true,
						"No supported refresh rates were found",
						"No supported refresh rates"
					);
				}
			}
			else
			{
				if ( refreshRates.Count == 0 )
				{
					return new Result(
						true,
						"No refresh rates provided",
						"No refresh rates provided"
					);
				}
			}

			//Increment the current refresh rate
			int currentRefreshRateIndex = refreshRates.IndexOf(currentDisplaySettings.dmDisplayFrequency);
			currentRefreshRateIndex = (currentRefreshRateIndex + 1) % refreshRates.Count;
			int refreshRate = refreshRates[currentRefreshRateIndex];

			//Set it
			return SetRefreshRate(refreshRate);
		}

		#endregion

		#region Utility Methods

		private static User32.DEVMODE ConstructDEVMODE ()
		{
			var devmode = new User32.DEVMODE();
			devmode.dmSize = (short) Marshal.SizeOf(typeof(User32.DEVMODE));

			return devmode;
		}

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