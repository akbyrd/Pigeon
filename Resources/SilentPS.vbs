'* Confirm that an argument was passed in
if WScript.Arguments.Count = 0 then
    WScript.Echo "Missing parameters"
	WScript.Quit
end if

'* Execute the passed in PowerShell script/command
command = "powershell -nologo -command "
for each arg in WScript.Arguments
	command = command & arg & " "
next

set shell = CreateObject("WScript.Shell")
shell.Run command,0