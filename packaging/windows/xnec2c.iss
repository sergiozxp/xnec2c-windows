#define MyAppName "Xnec2c"
#define MyAppVersion "4.4.18"
#define MyAppExeName "xnec2c-launcher.exe"

; File association support is intentionally disabled for this stage. Set this
; define to 1 only after the .nec association has been tested on Windows 11.
#define EnableNecAssociation 0

[Setup]
AppId={{E11469D2-32E4-411E-9C95-1AFA9BB82B9F}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher=KJ7LNW and Xnec2c contributors
AppPublisherURL=https://github.com/KJ7LNW/xnec2c
AppSupportURL=https://github.com/KJ7LNW/xnec2c/issues
AppUpdatesURL=https://github.com/KJ7LNW/xnec2c/releases
AppReadmeFile={app}\share\doc\xnec2c-windows\README-WINDOWS.md
DefaultDirName={localappdata}\Programs\Xnec2c
DefaultGroupName=Xnec2c
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0.22000
SourceDir=..\..\dist\xnec2c-windows-x64-ucrt64
OutputDir=..\installer
OutputBaseFilename=Xnec2c-{#MyAppVersion}-Windows-x64-Setup
SetupIconFile=launcher\xnec2c.ico
LicenseFile=..\..\COPYING
InfoBeforeFile=..\..\UPSTREAM.md
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
UsePreviousAppDir=yes
UsePreviousGroup=yes
UsePreviousTasks=yes
Uninstallable=yes
UninstallDisplayName={#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\{#MyAppExeName}
CloseApplications=yes
RestartApplications=no
RestartIfNeededByRun=no
AlwaysRestart=no
ChangesEnvironment=no
#if EnableNecAssociation
ChangesAssociations=yes
#else
ChangesAssociations=no
#endif
VersionInfoVersion=4.4.18.0
VersionInfoCompany=KJ7LNW and Xnec2c contributors
VersionInfoDescription=Xnec2c per-user installer
VersionInfoProductName={#MyAppName}
VersionInfoProductVersion={#MyAppVersion}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
#if EnableNecAssociation
Name: "fileassoc"; Description: "Associate .nec antenna model files with Xnec2c"; GroupDescription: "File associations:"; Flags: unchecked
#endif

[Files]
Source: "*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Xnec2c"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"
Name: "{group}\Uninstall Xnec2c"; Filename: "{uninstallexe}"
Name: "{userdesktop}\Xnec2c"; Filename: "{app}\{#MyAppExeName}"; WorkingDir: "{app}"; Tasks: desktopicon

#if EnableNecAssociation
[Registry]
Root: HKCU; Subkey: "Software\Classes\.nec"; ValueType: string; ValueName: ""; ValueData: "Xnec2c.nec"; Flags: uninsdeletevalue; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Xnec2c.nec"; ValueType: string; ValueName: ""; ValueData: "NEC antenna model"; Flags: uninsdeletekey; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Xnec2c.nec\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: "{app}\{#MyAppExeName},0"; Tasks: fileassoc
Root: HKCU; Subkey: "Software\Classes\Xnec2c.nec\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""; Tasks: fileassoc
#endif

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; WorkingDir: "{app}"; Flags: nowait postinstall skipifsilent
