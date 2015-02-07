; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

#define MyAppName "SLADE"
#define MyAppVersion "3.1.0.1"
#define VersionNum "3101"
#define MyAppURL "http://slade.mancubus.net"
#define MyAppExeName "SLADE.exe"

; TODO:
; - Add option to install portable version

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
; Do not use the same AppId value in installers for other applications.
; (To generate a new GUID, click Tools | Generate GUID inside the IDE.)
AppId={{3EFD0AA9-5156-40DB-9646-360180FF5DFA}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
;AppVerName={#MyAppName} {#MyAppVersion}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
InfoBeforeFile=..\dist\slade3.txt
OutputBaseFilename=Setup_{#MyAppName}_{#VersionNum}
SetupIconFile=..\slade.ico
Compression=lzma
SolidCompression=yes

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
;Name: "de"; MessagesFile: "compiler:Languages\German.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 0,6.1

[Files]
Source: "..\dist\SLADE.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\FreeImage.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libfluidsynth.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\libsndfile-1.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\openal32.dll"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\slade.pk3"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\slade3.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\dist\SLADE.pdb"; DestDir: "{app}"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{commondesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

#include "scripts\products.iss"

#include "scripts\products\stringversion.iss"
#include "scripts\products\winversion.iss"
#include "scripts\products\fileversion.iss"
#include "scripts\products\dotnetfxversion.iss"

#include "scripts\products\vcredist2013.iss"

[Code]
function InitializeSetup(): boolean;
begin
	//init windows version
	initwinversion();

  // Check for VS2012 runtimes installed
  vcredist2013();

  Result := true;
end;
