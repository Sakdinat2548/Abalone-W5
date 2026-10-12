; Abalone W5 — Windows installer (Inno Setup 6).
;
; Built in CI, never by hand: release.yml and nightly.yml run iscc with
; /DAppVersion=<x.y.z[-nightly]> and /DOutFile=<setup-name> from the repo
; root (/O"." puts the exe next to the zips). Relative paths below resolve
; from this script's own directory either way.
;
; Unsigned (free-plugin standard: SmartScreen warns on first run). Static
; MSVC runtime (/MT, see AGENTS.md) so no VC redist is bundled or needed.
; VST3 lives at {commoncf}\VST3 by spec — no directory page, average-joe-proof.
; The uninstaller lives under Program Files (NOT loose in the shared VST3
; folder — that pattern collides across vendors' unins000.* and drags
; shared-scan-folder weirdness into every VST3). DAWs only scan *.vst3
; dirs, so both locations are inert to them.
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
DefaultDirName={autopf}\Abalone W5
DisableDirPage=yes
OutputBaseFilename={#OutFile}
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
LicenseFile=..\LICENSE
WizardStyle=modern
UninstallDisplayName=Abalone W5 {#AppVersion}
; A running DAW locks the .vst3: offer to close it instead of failing on
; the locked file. RestartApplications=no is explicit (Inno 6 defaults it
; to yes): a closed DAW must NEVER auto-restart — it may hold unsaved work.
CloseApplications=yes
RestartApplications=no

[Files]
Source: "..\build\AbaloneW5_artefacts\Release\VST3\Abalone-W5.vst3\*"; DestDir: "{commoncf}\VST3\Abalone-W5.vst3"; Flags: recursesubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf}\VST3\Abalone-W5.vst3"
