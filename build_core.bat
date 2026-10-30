@ECHO OFF
SETLOCAL
SET "ROOT=%~dp0"
SET "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
SET "MSB=%VSDIR%\MSBuild\Current\Bin\MSBuild.exe"
SET "ATLMFC=%VSDIR%\VC\Tools\MSVC\14.51.36231\atlmfc"
SET "LOG=%ROOT%build_core.log"

IF NOT EXIST "%MSB%" GOTO NoMSB
IF NOT EXIST "%ATLMFC%\include\afx.h" GOTO NoATLMFC

FOR /F "TOKENS=2 DELIMS=:" %%A IN ('CHCP') DO SET "OLDCP=%%A"
SET "OLDCP=%OLDCP: =%"
CHCP 65001 >NUL
SET "VSLANG=1033"

ECHO Building libspp2pgs then Spp2Pgs.exe (Release x64, toolset v145)
ECHO.
"%MSB%" "%ROOT%Spp2Pgs\Spp2Pgs.vcxproj" /nologo /m /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VisualStudioVersion=18.0 /p:SolutionDir=%ROOT% /verbosity:minimal "/flp:LogFile=%LOG%;Encoding=UTF-8;Verbosity=minimal"
SET "RC=%ERRORLEVEL%"
ECHO.
IF NOT "%RC%"=="0" GOTO Failed
ECHO BUILD OK, look in the bin folder.
GOTO Done
:Failed
ECHO BUILD FAILED, full log: %LOG%
GOTO Done
:NoMSB
ECHO MSBuild not found: %MSB%
GOTO Done
:NoATLMFC
ECHO MFC/ATL headers not found: %ATLMFC%\include\afx.h
:Done
IF DEFINED OLDCP CHCP %OLDCP% >NUL
cmd /k
