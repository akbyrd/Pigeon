namespace GameSettings
{
	using System;
	using System.Collections.Generic;
	using System.Linq;
	using CommandLine;
	//using CoreAudioApi;

	//TODO: Add Audio Device support
	//TODO: Implement proper notification

	internal class Program
	{
		static private int Main ( string[] args )
		{
			//Parse command line options
			var parsingResult = Parser.Default.ParseArguments<Options>(args);

			/* Abort on any errors. Help info is printed to the command line
			 * automatically by Parser.
			 */
			if ( parsingResult.Errors.Any() ) { return -1; }

			//Execute functionality based on command line params
			Options options = parsingResult.Value;
			var results = ProcessOptions(options);

			//Handle output
			int ret = 0;
			foreach ( var result in results )
			{
				if ( result.IsError ) { ret = -1; }

				if ( result.Notify && options.Toast )
					Notification.Notify(result.ShortOutput);

				if ( result.IsError || options.Verbose )
					Console.WriteLine(result.LongOutput);
			}

			return ret;
		}

		#region Command Line Parsing

		private class Options
		{
			//[CommandLine.Option('f', "force")]
			//public bool Force { get; set; }

			[Option('v', "verbose")]
			public bool Verbose { get; set; }

			private const string ToastHelp = "Display a short toast notification with results.";
			[Option('t', "toast")]
			public bool Toast { get; set; }

			private const string RRSetName = "Refresh";
			private const string RRToggleHelp = "Cycles through provided refresh rates at the current resolution. If none are provided, cycles through all supported refresh rates at the current resolution.";
			[Option('r', "ToggleRefreshRate", Min = 0, DefaultValue = null, HelpText = RRToggleHelp, SetName = RRSetName)]
			public IEnumerable<int> ToggleRefreshRate { get; set; }

			private const string RRSetHelp = "Set refresh rate to the provided value. The current resolution is maintained.";
			[Option('s', "SetRefreshRate", SetName = RRSetName, HelpText = RRSetHelp)]
			public int? SetRefreshRate { get; set; }

			private const string ADSetName = "AudioDevice";
			private const string ADToggleHelp = "Cycles through provided audio playback devices. If none are provided, cycles through all enabled playback devices.";
			[Option('a', "ToggleAudioDevice", Min = 0, DefaultValue = null, HelpText = ADToggleHelp, SetName = ADSetName)]
			public IEnumerable<string> ToggleAudioDevice { get; set; }

			private const string ADSetHelp = "Set the current audio playback device to the provided value. Asterisks can be used as wildcards.";
			[Option('b', "SetAudioDevice", HelpText = ADSetHelp, SetName = ADSetName)]
			public string SetAudioDevice { get; set; }
		}

		#endregion

		#region Functionality

		private static IList<Result> ProcessOptions ( Options options )
		{
			var results = new List<Result>();

			var ret = ProcessRefreshRateOptions(options);
			results.AddRange(ret);

			ret = ProcessAudioDeviceOptions(options);
			results.AddRange(ret);

			return results;
		}

		#region Refresh Rate

		private static Lazy<DisplayController> DisplayController = new Lazy<DisplayController>();

		private static IList<Result> ProcessRefreshRateOptions ( Options options )
		{
			var results = new List<Result>();

			var ret = ProcessToggleRefreshRate(options);
			results.AddRange(ret);

			ret = ProcessSetRefreshRate(options);
			results.AddRange(ret);

			return results;
		}

		private static IList<Result> ProcessToggleRefreshRate ( Options options )
		{
			var results = new List<Result>();

			if ( options.ToggleRefreshRate != null )
			{
				int refreshRateCount = options.ToggleRefreshRate.Count();
				if ( refreshRateCount == 1 )
				{
					results.Add(new Result(
						true,
						"Toggling refresh rate requires either 0 or >1 values.",
						"Invalid args"
					));
				}
				else if ( refreshRateCount > 1 )
				{
					results.Add(new Result(
						false,
						"Toggling refresh rate to next value in set " + string.Join(", ", options.ToggleRefreshRate.Select(rr => rr.ToString()) + " Hz"),
						"Toggling refresh rate"
					));

					var ret = DisplayController.Value.ToggleRefreshRate();
					results.Add(ret);
				}
				else
				{
					results.Add(new Result(
						false,
						"Toggling refresh rate to next supported value.",
						"Toggling refresh rate"
					));

					var ret = DisplayController.Value.ToggleRefreshRate();
					results.Add(ret);
				}
			}

			return results;
		}

		private static IList<Result> ProcessSetRefreshRate ( Options options )
		{
			var results = new List<Result>();

			if ( options.SetRefreshRate.HasValue )
			{
				results.Add(new Result(
					false,
					"Setting refresh rate to " + options.SetRefreshRate + " Hz",
					"Refresh Rate: " + options.SetRefreshRate + " Hz"
				));

				var ret = DisplayController.Value.SetRefreshRate(options.SetRefreshRate.Value);
				results.Add(ret);
			}

			return results;
		}

		#endregion

		#region Audio Device

		private static IList<Result> ProcessAudioDeviceOptions ( Options options )
		{
			var results = new List<Result>();

			var ret = ProcessToggleAudioDevice(options);
			results.AddRange(ret);

			ret = ProcessSetAudioDevice(options);
			results.AddRange(ret);

			return results;
		}

		private static IList<Result> ProcessToggleAudioDevice ( Options options )
		{
			var results = new List<Result>();

			//MMDeviceEnumerator DevEnum = new MMDeviceEnumerator();
			//MMDeviceCollection devices = DevEnum.EnumerateAudioEndPoints(EDataFlow.eRender, EDeviceState.DEVICE_STATE_ACTIVE);
			//MMDevice DefaultDevice = DevEnum.GetDefaultAudioEndpoint(EDataFlow.eRender, ERole.eMultimedia);

			////Find current device
			//int index = -1;
			//for ( int i = 0; i < devices.Count; ++i )
			//{
			//	if ( devices[i].ID == DefaultDevice.ID )
			//	{
			//		index = i;
			//		break;
			//	}
			//}

			////Increment device
			//index = (index + 1) % devices.Count;

			////Set device
			//PolicyConfigClient client = new PolicyConfigClient();
			//client.SetDefaultEndpoint(devices[index].ID, ERole.eMultimedia);

			return results;
		}

		private static IList<Result> ProcessSetAudioDevice ( Options options )
		{
			var results = new List<Result>();

			//int index = 0;

			//if ( !string.IsNullOrEmpty(options.SetAudioDevice) )
			//{
			//	MMDeviceEnumerator DevEnum = new MMDeviceEnumerator();
			//	MMDeviceCollection devices = DevEnum.EnumerateAudioEndPoints(EDataFlow.eRender, EDeviceState.DEVICE_STATE_ACTIVE);

			//	PolicyConfigClient client = new PolicyConfigClient();
			//	client.SetDefaultEndpoint(devices[index].ID, ERole.eMultimedia);

			//	results.Add(new Result(
			//		false,
			//		"Set audio device",
			//		"Set audio device",
			//		true
			//	));
			//}

			return results;
		}

		#endregion

		#endregion
	}
}