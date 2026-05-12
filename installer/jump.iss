[Setup]
AppName=Jump
AppVersion={#MyAppVersion}
AppPublisher=RazMake
AppPublisherURL=https://github.com/RazMake/j-tool
AppSupportURL=https://github.com/RazMake/j-tool/issues
DefaultDirName={autopf}\Jump
DefaultGroupName=Jump
AllowNoIcons=yes
OutputDir=..\dist
OutputBaseFilename=jump-{#MyAppVersion}-win-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\jc.exe
WizardStyle=modern
PrivilegesRequired=lowest
ChangesEnvironment=yes

[Files]
Source: "..\build\j.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\build\jc.exe"; DestDir: "{app}"; Flags: ignoreversion

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Check: NeedsAddPath(ExpandConstant('{app}'))

[Run]
Filename: "{app}\jc.exe"; Parameters: "--install"; Description: "Configure shell integration (CMD, PowerShell)"; Flags: postinstall nowait skipifsilent shellexec

[UninstallRun]
Filename: "{app}\jc.exe"; Parameters: "--uninstall"; Flags: runhidden

[UninstallDelete]
Type: files; Name: "{app}\jump_j.cmd"
Type: files; Name: "{app}\j.exe.old"
Type: files; Name: "{app}\jc.exe.old"

[Code]
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_CURRENT_USER, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;
