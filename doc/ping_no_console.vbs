Dim Arg, address, var2
Set Arg = WScript.Arguments

Set WshShell = WScript.CreateObject("WScript.Shell")


address = Arg(0)
file_name = Arg(1)

return = WshShell.Run("cmd /c ping -w 500 -n 4 " & address & " > " & file_name, 0, true)
