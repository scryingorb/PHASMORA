#ifndef ProductVersion
  #define ProductVersion "1.0.0"
#endif
#ifndef ProjectRoot
  #define ProjectRoot ".."
#endif
#ifndef OutputRoot
  #define OutputRoot "..\dist\PHASMORA-1.0.0-Windows"
#endif
[Setup]
AppId={{E14C74E3-461B-48F2-AC84-D6F12F08D9F0}
AppName=PHASMORA
AppVersion={#ProductVersion}
AppPublisher=PHASMORA
AppVerName=PHASMORA {#ProductVersion}
VersionInfoVersion={#ProductVersion}
DefaultDirName={commoncf64}\VST3\PHASMORA.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
UsePreviousAppDir=no
PrivilegesRequired=admin
; Controlled test installs can explicitly select current-user mode and /DIR.
PrivilegesRequiredOverridesAllowed=commandline
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputRoot}
OutputBaseFilename=PHASMORA-{#ProductVersion}-Windows-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
Uninstallable=yes
UninstallDisplayName=PHASMORA {#ProductVersion}
UninstallFilesDir={app}\Uninstall
CloseApplications=no
RestartApplications=no
SetupLogging=yes
InfoBeforeFile={#ProjectRoot}\Installer\BeforeInstall.txt
[Files]
Source: "{#ProjectRoot}\build\GHOST_artefacts\Release\VST3\PHASMORA.vst3\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#ProjectRoot}\README.md"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#ProjectRoot}\LICENSE-PENDING.txt"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#ProjectRoot}\THIRD-PARTY-NOTICES.txt"; DestDir: "{app}\Documentation"; Flags: ignoreversion
Source: "{#ProjectRoot}\ThirdPartyLicenses\*"; DestDir: "{app}\Documentation\ThirdPartyLicenses"; Flags: ignoreversion recursesubdirs createallsubdirs
[Code]
var
  LegacyPath, LegacyBackup: String;
  LegacyMoved, InstallComplete: Boolean;

function GetFileAttributes(FileName: String): LongWord;
  external 'GetFileAttributesW@kernel32.dll stdcall';

function IsAllowedFile(Relative: String): Boolean;
begin
  Result := Pos('|' + Lowercase(Relative) + '|',
    '|contents\resources\moduleinfo.json|contents\x86_64-win\ghost.vst3|' +
    'documentation\readme.md|documentation\license-pending.txt|documentation\third-party-notices.txt|' +
    'documentation\thirdpartylicenses\flac-license.txt|documentation\thirdpartylicenses\harfbuzz-copying.txt|' +
    'documentation\thirdpartylicenses\jpeg-readme.txt|documentation\thirdpartylicenses\juce-license.md|' +
    'documentation\thirdpartylicenses\oggvorbis-license.txt|documentation\thirdpartylicenses\png-license.txt|' +
    'documentation\thirdpartylicenses\sheenbidi-license.txt|documentation\thirdpartylicenses\vst3-license.txt|' +
    'documentation\thirdpartylicenses\zlib-license.txt|uninstall\unins000.exe|uninstall\unins000.dat|uninstall\unins000.msg|') > 0;
end;

function IsAllowedDirectory(Relative: String): Boolean;
begin
  Result := Pos('|' + Lowercase(Relative) + '|',
    '|contents|contents\resources|contents\x86_64-win|documentation|documentation\thirdpartylicenses|uninstall|') > 0;
end;

function ScanLegacy(Folder, Relative: String): Boolean;
var
  Found: TFindRec;
  ChildRelative: String;
begin
  Result := True;
  if FindFirst(AddBackslash(Folder) + '*', Found) then begin
    try
      repeat
        if (Found.Name <> '.') and (Found.Name <> '..') then begin
          ChildRelative := Relative + Found.Name;
          { Never follow a junction or symbolic link, including nested directories. }
          if (Found.Attributes and $400) <> 0 then Result := False
          else if (Found.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            Result := IsAllowedDirectory(ChildRelative) and
              ScanLegacy(AddBackslash(Folder) + Found.Name, ChildRelative + '\')
          else Result := IsAllowedFile(ChildRelative);
          if not Result then Break;
        end;
      until not FindNext(Found);
    finally
      FindClose(Found);
    end;
  end;
end;

function RecognizedLegacy(Folder: String): Boolean;
var
  Metadata: AnsiString;
  Attr: LongWord;
begin
  Result := False;
  Attr := GetFileAttributes(Folder);
  if (Attr = $FFFFFFFF) or ((Attr and $400) <> 0) then Exit;
  if not FileExists(Folder + '\Contents\x86_64-win\GHOST.vst3') then Exit;
  if not LoadStringFromFile(Folder + '\Contents\Resources\moduleinfo.json', Metadata) then Exit;
  if (Pos('ABCDEF019182FAEB4768737447683031', String(Metadata)) = 0) or
     (Pos('ABCDEF011234ABCD4768737447683031', String(Metadata)) = 0) then Exit;
  { Known tested 1.0.0 binary. Names alone are never sufficient proof of ownership. }
  if CompareText(GetSHA256OfFile(Folder + '\Contents\x86_64-win\GHOST.vst3'),
    '0c3054ad5c1bb18f043b58803063d743d186c04343b6086f9fbe1b5039731a15') <> 0 then Exit;
  Result := ScanLegacy(Folder, '');
end;

function RemoveKnownTree(Folder: String): Boolean;
var
  Found: TFindRec;
begin
  Result := True;
  if FindFirst(AddBackslash(Folder) + '*', Found) then begin
    try
      repeat
        if (Found.Name <> '.') and (Found.Name <> '..') then begin
          if (Found.Attributes and $400) <> 0 then Result := False
          else if (Found.Attributes and FILE_ATTRIBUTE_DIRECTORY) <> 0 then
            Result := RemoveKnownTree(AddBackslash(Folder) + Found.Name)
          else Result := DeleteFile(AddBackslash(Folder) + Found.Name);
          if not Result then Break;
        end;
      until not FindNext(Found);
    finally
      FindClose(Found);
    end;
  end;
  if Result then Result := RemoveDir(Folder);
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  Result := '';
  LegacyPath := ExpandFileName(AddBackslash(ExtractFileDir(ExpandConstant('{app}'))) + 'GHOST.vst3');
  LegacyBackup := LegacyPath + '.phasmora-migration';
  if CompareText(LegacyPath, ExpandFileName(ExpandConstant('{app}'))) = 0 then begin
    Result := 'PHASMORA must use its own PHASMORA.vst3 destination.'; Exit;
  end;
  if DirExists(LegacyBackup) or FileExists(LegacyBackup) then begin
    Result := 'A previous migration backup exists. Review ' + LegacyBackup + ' before retrying.'; Exit;
  end;
  if DirExists(LegacyPath) and not RecognizedLegacy(LegacyPath) then
    Result := 'The existing GHOST.vst3 is unrecognized, modified, linked, or contains unexpected files. Nothing was removed. Review it manually before installing PHASMORA.';
  if FileExists(LegacyPath) then
    Result := 'An unrecognized file named GHOST.vst3 exists. Review it manually; nothing was removed.';
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if (CurStep = ssInstall) and DirExists(LegacyPath) then begin
    if not RecognizedLegacy(LegacyPath) then RaiseException('Legacy bundle changed during installation.');
    if not RenameFile(LegacyPath, LegacyBackup) then
      RaiseException('Cannot stage the old bundle. Close your DAW and retry.');
    LegacyMoved := True;
  end;
  if CurStep = ssPostInstall then begin
    InstallComplete := True;
    if LegacyMoved then begin
      { Only the explicitly verified sibling path and known file set are removed. }
      if RecognizedLegacy(LegacyBackup) and RemoveKnownTree(LegacyBackup) then
        Log('Verified legacy GHOST bundle migrated and removed.')
      else begin
        Log('Old bundle backup retained for manual review: ' + LegacyBackup);
        if not WizardSilent then MsgBox('PHASMORA is installed. A legacy backup remains at ' + LegacyBackup + '. Close your DAW and review it manually.', mbInformation, MB_OK);
      end;
    end;
  end;
end;

procedure DeinitializeSetup();
begin
  if LegacyMoved and not InstallComplete and DirExists(LegacyBackup) then begin
    if not DirExists(LegacyPath) then
      if RenameFile(LegacyBackup, LegacyPath) then Log('Restored legacy bundle after unsuccessful installation.')
      else Log('Restore failed; legacy files retained at ' + LegacyBackup);
  end;
end;

function InitializeSetup(): Boolean;
begin
  Result := True;
  if not WizardSilent then
    Result := MsgBox('Close your DAW before installing PHASMORA. Continue?', mbConfirmation, MB_YESNO) = IDYES;
end;
