namespace GameSettings
{
	using System.Runtime.CompilerServices;

	internal class Result
	{
		public readonly bool   IsError;
		public readonly string ShortOutput;
		public readonly string LongOutput;

		public bool Notify { get { return notify || IsError; } }
		private readonly bool notify;

		public Result ( bool isError, string longOutput, string shortOutput, bool notify = false, [CallerMemberName] string caller = null )
		{
			    IsError = isError;
			 LongOutput = caller + " - " + longOutput;
			ShortOutput = shortOutput;
			this.notify = notify;
		}
	}
}