namespace GameSettings
{
	internal partial class DisplayController
	{
		public class Resolution
		{
			public int width;
			public int height;
			public int refreshRate;

			public Resolution ( int width, int height, int refreshRate )
			{
				this.width = width;
				this.height = height;
				this.refreshRate = refreshRate;
			}
		}
	}
}