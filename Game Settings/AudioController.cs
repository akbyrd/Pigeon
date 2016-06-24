namespace GameSettings
{
	using CoreAudioApi;

	internal class AudioController
	{
		public Result ToggleAudioDevice ()
		{
			MMDeviceEnumerator DevEnum = new MMDeviceEnumerator();
			MMDeviceCollection devices = DevEnum.EnumerateAudioEndPoints(EDataFlow.eRender, EDeviceState.DEVICE_STATE_ACTIVE);
			MMDevice DefaultDevice = DevEnum.GetDefaultAudioEndpoint(EDataFlow.eRender, ERole.eMultimedia);

			//Find current device
			int index = -1;
			for ( int i = 0; i < devices.Count; ++i )
			{
				if ( devices[i].ID == DefaultDevice.ID )
				{
					index = i;
					break;
				}
			}

			//Increment device
			index = (index + 1) % devices.Count;

			//Set device
			PolicyConfigClient client = new PolicyConfigClient();
			client.SetDefaultEndpoint(devices[index].ID, ERole.eMultimedia);

			return new Result(
				false,
				"Set audio device",
				"Set audio device",
				true
			);
		}
	}
}