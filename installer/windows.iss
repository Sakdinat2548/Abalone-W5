; Abalone W5 — Windows installer (Inno Setup 6).
;
; Built in CI, never by hand: release.yml and nightly.yml run iscc with
; /DAppVersion=<x.y.z[-nightly]> and /DOutFile=<setup-name> from the repo
; root (/O"." puts the exe next to the zips). Relative paths below resolve
; from this script's own directory either way.
;
; Unsigned (free-plugin standard: SmartScreen warns on first run). Static
; MSVC runtime (/MT, see AGENTS.md) so no VC redist is bundled or needed.
; VST3 lives at {commoncf}\VST3 by spec (Steinberg: "must comply") — that is
; the default below. The folder page stays visible anyway (industry
; standard: Terminal, HISE, KVR templates all show it): Reaper/FL users
; with custom scan folders can point elsewhere, everyone else clicks Next.
; The uninstaller always lives under Program Files regardless of the VST3
; choice (UninstallFilesDir), never loose in the shared scan folder.
; Signed one day: add SignTool=signtool below and pass
; /Ssigntool="signtool.exe sign ..." to iscc in CI.
#ifndef AppVersion
#define AppVersion "1.3.0"
#endif
#ifndef OutFile
#define OutFile "Abalone-W5-" + AppVersion + "-windows-setup"
#endif

[Setup]
AppId={{E0284EAD-CEE4-418D-9728-671113869EA1}
AppName=Abalone W5
AppVersion={#AppVersion}
AppPublisher=Sakdinat2548
AppPublisherURL=https://github.com/Sakdinat2548/Abalone-W5
DefaultDirName={commoncf}\VST3
; The Ready page hides the destination unless told otherwise — average
; joes should see where it goes right before Install.
AlwaysShowDirOnReadyPage=yes
; The destination is a SHARED scan folder that always exists on DAW
; machines — never warn about that (the warning reads as an error to
; average joes, and merging in is the entire point).
DirExistsWarning=no
UninstallFilesDir={autopf}\Abalone W5
OutputBaseFilename={#OutFile}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=..\LICENSE
InfoBeforeFile=install-notes.txt
WizardStyle=modern
WizardSmallImageFile=wizard-small.bmp
SetupIconFile=app.ico
UninstallDisplayIcon={autopf}\Abalone W5\app.ico
UninstallDisplayName=Abalone W5 {#AppVersion}
; A running DAW locks the .vst3: offer to close it instead of failing on
; the locked file. RestartApplications=no is explicit (Inno 6 defaults it
; to yes): a closed DAW must NEVER auto-restart — it may hold unsaved work.
CloseApplications=yes
RestartApplications=no

[Files]
Source: "..\build\AbaloneW5_artefacts\Release\VST3\Abalone-W5.vst3\*"; DestDir: "{app}\Abalone-W5.vst3"; Flags: recursesubdirs ignoreversion
; Icon backing: the Add/Remove Programs entry points at this copy.
Source: "app.ico"; DestDir: "{autopf}\Abalone W5"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Abalone-W5.vst3"
