; Script generated by the Inno Setup Script Wizard.
; SEE THE DOCUMENTATION FOR DETAILS ON CREATING INNO SETUP SCRIPT FILES!

[Setup]
AppName=RMAExpress
AppVerName=RMAExpress 1.2.0 alpha 3 (1.20.0-alpha-3)    
AppPublisher=RMAExpress
AppPublisherURL=http://rmaexpress.bmbolstad.com
AppSupportURL=http://rmaexpress.bmbolstad.com
AppUpdatesURL=http://rmaexpress.bmbolstad.com
DefaultDirName={pf64}\RMAExpress
DefaultGroupName=RMAExpress
WizardImageStretch=no
WizardImageFile="C:\Users\bmb\Development\RMAExpress\RMAExpress_MasterLOGO_Installer.bmp"
WizardSmallImageFile="C:\Users\bmb\Development\RMAExpress\RMAExpress_MasterLOGOSmall.bmp"
WizardImageBackColor=clWhite
AppCopyright=Copyright (C) 2003-2015 B. M. Bolstad.
BackSolid=yes
ArchitecturesInstallIn64BitMode=x64
ArchitecturesAllowed=x64
OutputBaseFilename=RMAExpress_Setup_x64

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; GroupDescription: "Additional icons:"

[Files]
Source: "C:\Users\bmb\Development\RMAExpress\vc.proj\x64\Release\RMAExpress.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\bmb\Development\RMAExpress\vc.proj\x64\Release\RMADataConv.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\bmb\Development\RMAExpress\vc.proj\x64\Release\RMAExpressConsole.exe"; DestDir: "{app}"; Flags: ignoreversion
;Source: "\\Bmbbox\tmp\RMAExpress\RMAExpress_UsersGuide.pdf";  DestDir: "{app}"; Flags: ignoreversion
Source: "C:\Users\bmb\Development\vcredist_x64.exe"; DestDir: "{app}"; Flags: ignoreversion
; NOTE: Don't use "Flags: ignoreversion" on any shared system files

[Icons]
Name: "{group}\RMAExpress"; Filename: "{app}\RMAExpress.exe"
Name: "{group}\RMADataConv"; Filename: "{app}\RMADataConv.exe"
;Name: "{group}\RMAExpress Users Guide"; Filename: "{app}\RMAExpress_UsersGuide.pdf"
Name: "{userdesktop}\RMAExpress"; Filename: "{app}\RMAExpress.exe"; Tasks: desktopicon
Name: "{userdesktop}\RMADataConv"; Filename: "{app}\RMADataConv.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\vcredist_x64.exe";  Parameters: "/quiet"; Flags:
Filename: "{app}\RMAExpress.exe"; Description: "Launch RMAExpress"; Flags: nowait postinstall skipifsilent