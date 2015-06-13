namespace GameSettings
{
	using System;
	using System.Threading;
	using System.Windows.Forms;
	using GameSettings.Properties;

	internal class Notification
	{
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

		public static void Notify ( string text )
		{

		}
	}
}