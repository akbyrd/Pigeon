#TODO
#	Better notification of changes (overwrite previous note, notify failure, clean up parameters)
#	Partial name matching

Param (
	[ValidateSet("ToggleRefresh", "SetRefresh", "ToggleAudio", "SetAudio")]
	[Parameter(Mandatory = $true, Position = 0, ParameterSetName = "ToggleSet")]
	[String]$action
)

DynamicParam
{
	# Common setup
	$paramDictionary = New-Object `
		-Type System.Management.Automation.RuntimeDefinedParameterDictionary

	$attributes = New-Object System.Management.Automation.ParameterAttribute
	$attributes.ParameterSetName = "__AllParameterSets"
	$attributes.Mandatory = $true
	$attributes.Position = 1

	$attributeCollection = New-Object `
		-Type System.Collections.ObjectModel.Collection[System.Attribute]
	$attributeCollection.Add($attributes)

	if ( $action -eq "SetAudio" ) {
		# audioDevice value
		$dynParam1 = New-Object `
			-Type System.Management.Automation.RuntimeDefinedParameter("audioDevice", [String], $attributeCollection)

		$paramDictionary.Add("audioDevice", $dynParam1)
	}

	if ( $action -eq "SetRefresh" ) {
		# force switch
		$attributes2 = New-Object System.Management.Automation.ParameterAttribute
		$attributes2.ParameterSetName = "__AllParameterSets"
		$attributes2.Mandatory = $false
		$attributeCollection2 = New-Object `
			-Type System.Collections.ObjectModel.Collection[System.Attribute]
		$attributeCollection2.Add($attributes2)

		$dynParam2 = New-Object `
			-Type System.Management.Automation.RuntimeDefinedParameter("force", [switch], $attributeCollection2)

		$paramDictionary.Add("force", $dynParam2)

		# refreshRate value
		$arguments = @()
		$arguments += 1
		$arguments += 120
		$validation = New-Object `
			-Type System.Management.Automation.ValidateRangeAttribute -ArgumentList $arguments

		$attributeCollection.Add($validation)
		
		$dynParam3 = New-Object `
			-Type System.Management.Automation.RuntimeDefinedParameter("refreshRate", [Byte], $attributeCollection)

		$paramDictionary.Add("refreshRate", $dynParam3)
	}

	return $paramDictionary
}

begin {
	# I have no idea why this works, but marking the second block as "begin" doesn't....
	$audioDevice = $PSBoundParameters.audioDevice
	$refreshRate = $PSBoundParameters.refreshRate
	$force = $PSBoundParameters.force -ne $null

	$icon = $PSScriptRoot + "\Info.ico"
}

end {
	function ToggleAudioDevice
	{
		Import-Module AudioDeviceCmdlets
		$speakerName = "Speakers (Realtek High Definition Audio)"
		$headsetName = "Headset (4- SteelSeries H Wireless)"

		$currentDeviceName = (Get-DefaultAudioDevice).DeviceFriendlyName

		if ( $currentDeviceName -eq $speakerName ) {
			$audioDevice = $headsetName
		} else {
			$audioDevice = $speakerName
		}

		SetAudioDevice
	}

	function SetAudioDevice
	{
		Import-Module AudioDeviceCmdlets

		Set-DefaultAudioDevice -Type Multimedia $audioDevice

		$audioDevice -match "(?<=\().+(?=\))" |Out-Null

		ShowNotification $icon "Info" $matches[0] "Audio Output"
	}

	function ToggleRefreshRate
	{
		Import-Module DisplaySettings

		$currentDisplaySettings = (Get-DisplaySettings)

		if ( $currentDisplaySettings.ScreenRefreshRate -ne 120 ) {
			$refreshRate = 120
		} else {
			$refreshRate = 60
		}
		$force = $true
		SetRefreshRate
	}

	function SetRefreshRate
	{
		Import-Module DisplaySettings

		$currentDisplaySettings = (Get-DisplaySettings)

		Set-DisplaySettings -Force:$force -NoPrompt $currentDisplaySettings.Width $currentDisplaySettings.Height -frequency $refreshRate

		ShowNotification $icon "Info" ([string]$refreshRate + " Hz") "Refresh Rate"
	}

	function ShowNotification
	{
		Param (
			[Parameter(Mandatory = $true, Position = 0)]
			[String]$icon,

			[Parameter(Mandatory = $true, Position = 1)]
			[String]$tipIcon,

			[Parameter(Mandatory = $true, Position = 2)]
			[String]$tipText,

			[Parameter(Mandatory = $true, Position = 3)]
			[String]$tipTitle
		)

		[void] [System.Reflection.Assembly]::LoadWithPartialName("System.Windows.Forms")

		$objNotifyIcon = New-Object System.Windows.Forms.NotifyIcon 

		$objNotifyIcon.Icon = $icon
		#$objNotifyIcon.BalloonTipIcon = $tipIcon
		$objNotifyIcon.BalloonTipText = $tipText
		#$objNotifyIcon.BalloonTipTitle = $tipTitle
 
		$objNotifyIcon.Visible = $true 
		$objNotifyIcon.ShowBalloonTip(1000)
	}

	if ( $action -eq "ToggleRefresh" ) {
		ToggleRefreshRate
	} elseif ( $action -eq "SetRefresh" ) {
		SetRefreshRate
	} elseif ( $action -eq "ToggleAudio" ) {
		ToggleAudioDevice
	} elseif ( $action -eq "SetAudio" ) {
		SetAudioDevice
	}
}