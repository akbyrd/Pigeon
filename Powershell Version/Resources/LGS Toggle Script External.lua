audioDevice = false;
refreshRate = false;
lcdMessageTime = 2000;

function ToggleAudioDevice()
	audioDevice = not audioDevice;

	if ( audioDevice ) then
		PlayMacro("Set Audio Device Speakers");
		SetBacklightColor(255, 0, 0, "kb");
	else
		PlayMacro("Set Audio Device Headset");
		SetBacklightColor(0, 255, 0, "kb");
	end

	--NOTE: nircmd setdefaultsounddevice "Headset"
end

function ToggleRefreshRate()
	ClearLCD();
	refreshRate = not refreshRate;

	if ( refreshRate ) then
		PlayMacro("Set Refresh Rate 60");
		OutputLCDMessage("Refresh Rate Set To 60 Hz", lcdMessageTime);
	else
		PlayMacro("Set Refresh Rate 120");
		OutputLCDMessage("Refresh Rate Set To 120 Hz", lcdMessageTime);
	end

	--NOTE: setres f120 n
end

function OnEvent(event, arg, family)
--OutputLogMessage("event = %s, arg = %s, family = %s\n", event, arg, family)
	--Keyboard
	if ( family == "mouse" ) then
		if ( event == "MOUSE_BUTTON_PRESSED" ) then

			--G7
			if ( arg == 7 ) then
				if ( IsModifierPressed("ctrl") ) then
					ToggleAudioDevice();
				end
			--G7

			--G8
			elseif ( arg == 8 ) then
				if ( IsModifierPressed("ctrl") ) then
					ToggleRefreshRate();
				end
			end
			--G8
		end
	end
	--Keyboard
end

function Initialize()
	Sleep(5000); --Give PC a chance to start up.

	ToggleAudioDevice();
	Sleep(100);
	ToggleRefreshRate();
end

--Initialize();