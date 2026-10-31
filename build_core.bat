@ECHO OFF
SETLOCAL
SET "ROOT=%~dp0"
SET "EXITCODE=0"
SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
SET "VSDIR="
IF EXIST "%VSWHERE%" FOR /F "USEBACKQ TOKENS=*" %%I IN (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.ATLMFC -property installationPath`) DO SET "VSDIR=%%I"
IF DEFINED VSDIR GOTO HaveVS
IF NOT EXIST "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools" GOTO NoMSB
SET "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
:HaveVS
SET "MSB=%VSDIR%\MSBuild\Current\Bin\MSBuild.exe"
SET "ATLMFC="
FOR /D %%D IN ("%VSDIR%\VC\Tools\MSVC\*") DO IF EXIST "%%D\atlmfc\include\afx.h" SET "ATLMFC=%%D\atlmfc"
SET "LOG=%ROOT%build_core.log"

IF NOT EXIST "%MSB%" GOTO NoMSB
IF NOT DEFINED ATLMFC GOTO NoATLMFC

FOR /F "TOKENS=2 DELIMS=:" %%A IN ('CHCP') DO SET "OLDCP=%%A"
SET "OLDCP=%OLDCP: =%"
CHCP 65001 >NUL
SET "VSLANG=1033"

ECHO Visual Studio: %VSDIR%
ECHO MFC: %ATLMFC%
ECHO Building libspp2pgs then Spp2Pgs.exe (Release x64, toolset v145)
IF NOT EXIST "%ROOT%bin" MKDIR "%ROOT%bin"
ECHO.
"%MSB%" "%ROOT%Spp2Pgs\Spp2Pgs.vcxproj" /nologo /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VisualStudioVersion=18.0 /p:SolutionDir=%ROOT% /verbosity:minimal "/flp:LogFile=%LOG%;Encoding=UTF-8;Verbosity=minimal"
SET "RC=%ERRORLEVEL%"
ECHO.
IF NOT "%RC%"=="0" GOTO Failed
ECHO BUILD OK, look in the bin folder.
GOTO Done
:Failed
ECHO BUILD FAILED, full log: %LOG%
SET "EXITCODE=1"
GOTO Done
:NoMSB
ECHO MSBuild not found under "%VSDIR%".
SET "EXITCODE=1"
GOTO Done
:NoATLMFC
ECHO MFC/ATL headers not found under "%VSDIR%\VC\Tools\MSVC".
ECHO Install the "C++ MFC for latest v145 build tools" component.
SET "EXITCODE=1"
:Done
IF DEFINED OLDCP CHCP %OLDCP% >NUL
IF DEFINED CI EXIT /B %EXITCODE%
cmd /k
