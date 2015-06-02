namespace GameSettings
{
	using System;
	using System.Collections.Generic;
	using System.Linq;
	using CommandLine;

	//TODO: System tray notification
	//TODO: Option for notification
	//TODO: Add Audio Device support

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

				Console.WriteLine(result.LongOutput);
			}

			return ret;
		}

		#region Command Line Parsing

		private class Options
		{
			//[CommandLine.Option('e', "useErrorDialogs")]
			//public bool UseErrorDialogs { get; set; }

			//[CommandLine.Option('f', "force")]
			//public bool Force { get; set; }

			//TODO: Other syntax
			//      RefreshRate-Set, RefreshRate-Toggle, RefreshRate-ToggleGroup

			private const string RRSetName = "Refresh";
			private const string RRToggleHelp = "Cycles through up to 5 provided refresh rates at the current resolution. If none are provided, cycles through all supported refresh rates at the current resolution.";
			[Option('r', "ToggleRefreshRate", Min = 0, Max = 5, DefaultValue = null, HelpText = RRToggleHelp, SetName = RRSetName)]
			public IEnumerable<int> ToggleRefreshRate { get; set; }

			private const string RRSetHelp = "Set refresh rate to the provided value. The current resolution is maintained.";
			[Option('s', "SetRefreshRate", SetName = RRSetName, HelpText = RRSetHelp)]
			public int? SetRefreshRate { get; set; }

			//private const string ADSetName = "AudioDevice";
			//[CommandLine.Option('a', "ToggleAudioDevice", SetName = ADSetName)]
			//public bool? ToggleAudioDevice { get; set; }

			//[CommandLine.Option('b', "SetAudioDevice", SetName = ADSetName)]
			//public string SetAudioDevice { get; set; }
		}

		private class Result
		{
			public readonly bool   IsError;
			public readonly string ShortOutput;
			public readonly string LongOutput;

			public Result ( bool isError, string longOutput, string shortOutput = null )
			{
				IsError = isError;
				LongOutput = longOutput;
				ShortOutput = shortOutput;
			}
		}

		#endregion

		#region Functionality

		private static IEnumerable<Result> ProcessOptions ( Options options )
		{
			var results = new List<Result>();

			var ret = ProcessRefreshRateOptions(options);
			results.AddRange(ret);

			return results;
		}

		#region Refresh Rate

		private static Lazy<DisplayController> DisplayController = new Lazy<DisplayController>();

		private static IEnumerable<Result> ProcessRefreshRateOptions ( Options options )
		{
			var results = new List<Result>();

			var ret = ProcessToggleRefreshRate(options);
			results.AddRange(ret);

			ret = ProcessSetRefreshRate(options);
			results.AddRange(ret);

			return results;
		}

		private static IEnumerable<Result> ProcessToggleRefreshRate ( Options options )
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
						"Toggling refresh rate to next value in set " + string.Join(", ", options.ToggleRefreshRate.Select(rr => rr.ToString()))
					));

					//TODO: Handle output
					DisplayController.Value.ToggleRefreshRate();
				}
				else
				{
					results.Add(new Result(
						false,
						"Toggling refresh rate to next supported value."
					));

					//TODO: Handle output
					DisplayController.Value.ToggleRefreshRate();
				}
			}

			return results;
		}

		private static IEnumerable<Result> ProcessSetRefreshRate ( Options options )
		{
			var results = new List<Result>();

			if ( options.SetRefreshRate.HasValue )
			{
				results.Add(new Result(
					false,
					"Setting refresh rate to " + options.SetRefreshRate
				));

				//TODO: Handle output
				DisplayController.Value.SetRefreshRate(options.SetRefreshRate.Value);
			}

			return results;
		}

		#endregion

		#endregion

		//TODO: Move notification handling to its own class
		#region Notification
		/*
		private static NotifyIcon notification;

		private static void ShowIcon ()
		{
			if ( notification == null )
				notification = new NotifyIcon();

			notification.BalloonTipIcon = ToolTipIcon.Info;
			notification.BalloonTipText = "This is a NotifyIcon";
			notification.BalloonTipTitle = "NotifyIcon";
			notification.Icon = Resources.Info;
			notification.Visible = true;

			notification.BalloonTipClosed += ( s, e ) => notification.Dispose();

			notification.ShowBalloonTip(1000);

			Thread thread = new Thread(new ThreadStart(() => {
				Thread.Sleep(1000 + 8000);
				notification.Dispose();
			}));
			thread.Start();
			Console.WriteLine("Thread started");
		}

		private static void HideNotification ()
		{
			if ( notification != null )
				notification.Visible = false;
		}
		*/
		#endregion
	}
}