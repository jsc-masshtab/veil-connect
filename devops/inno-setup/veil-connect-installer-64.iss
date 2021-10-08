﻿; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "VeiL Connect"
#define MyAppVersion "&&VER&&.x64"
#define MyAppPublisher "JSC Research institute Masshtab"
#define MyAppURL "https://veil.mashtab.org"
#define MyAppExeName "veil_connect.exe"
#define MyAppIcoName "veil-connect.ico"
#define MyBuildVersion "&&BUILD_VER&&"

[Setup]
; NOTE: The value of AppId uniquely identifies this application. Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{60C768F1-3FE9-4C8E-A9A2-0EE659FDB2BE}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
VersionInfoVersion={#MyBuildVersion}
DefaultDirName={code:GetProgramFiles}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
; The [Icons] "quicklaunchicon" entry uses {userappdata} but its [Tasks] entry has a proper IsAdminInstallMode Check.
UsedUserAreasWarning=no
; Uncomment the following line to run in non administrative install mode (install for current user only.)
;PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog
OutputDir=C:\Jenkins\workspace\vdi-connect
OutputBaseFilename=veil-connect-installer
SetupIconFile=C:\Jenkins\workspace\vdi-connect\doc\veil-connect.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
Name: "app"; Description: "VeiL Connect"; Types: full custom; Flags: fixed
Name: "freerdp"; Description: "FreeRDP libraries"; Types: full custom; Flags: fixed
Name: "freerdp\232"; Description: "2.3.2 (Media Foundation)"; Types: full custom; Flags: exclusive
Name: "freerdp\220"; Description: "2.2.0 (OpenH264)"; Types: custom; Flags: exclusive
Name: "usbdk"; Description: "Spice USB Development Kit (UsbDK)"; Types: full

[Languages]
; Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}";
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode

[Files]
; Folders
Source: "C:\Jenkins\workspace\vdi-connect\build\lib\*"; DestDir: "{app}\lib"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\locale\*"; DestDir: "{app}\locale"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\log"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\rdp_data\*"; DestDir: "{app}\rdp_data"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\share\*"; DestDir: "{app}\share"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\x2go_data\*"; DestDir: "{app}\x2go_data"; Flags: ignoreversion recursesubdirs createallsubdirs; Components: app
; FreeRDP
Source: "C:\Jenkins\workspace\vdi-connect\build\freerdp_2.3.2\*.dll"; DestDir: "{app}"; Flags: ignoreversion deleteafterinstall; Components: freerdp\232
Source: "C:\Jenkins\workspace\vdi-connect\build\freerdp_2.2.0\*.dll"; DestDir: "{app}"; Flags: ignoreversion deleteafterinstall; Components: freerdp\220
; Files
Source: "C:\Jenkins\workspace\vdi-connect\build\veil_connect.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\vc_redist.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\usbdk.msi"; DestDir: "{tmp}"; Flags: deleteafterinstall; Components: usbdk
Source: "C:\Jenkins\workspace\vdi-connect\build\gspawn-win64-helper*.exe"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\*.dll"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\*.css"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\*.bak"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\*.vbs"; DestDir: "{app}"; Flags: ignoreversion; Components: app
Source: "C:\Jenkins\workspace\vdi-connect\build\*.ico"; DestDir: "{app}"; Flags: ignoreversion; Components: app

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; IconFilename: "{app}\{#MyAppIcoName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent
Filename: "{tmp}\vc_redist.exe"; Parameters: "/q /norestart"; Check: VCRedistNeedsInstall; Flags: waituntilterminated
Filename: "msiexec.exe"; Parameters: "/i ""{tmp}\usbdk.msi"" /qb"; WorkingDir: {tmp}; Check: UsbDkNeedsInstall; Flags: waituntilterminated

[Code]
function VCRedistNeedsInstall: Boolean;
begin
  Result := Not RegKeyExists(HKEY_LOCAL_MACHINE, 'SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X64');
end;

function UsbDkNeedsInstall: Boolean;
begin
  Result := WizardIsComponentSelected('usbdk') and Not FileExists('C:\Program Files\UsbDk Runtime Library\UsbDkController.exe');
end;

function GetProgramFiles(Param: string): string;
begin
  if IsWin64 then Result := ExpandConstant('{pf64}')
    else Result := ExpandConstant('{pf32}')
end;
