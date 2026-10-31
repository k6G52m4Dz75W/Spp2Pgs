@ECHO OFF
SETLOCAL
SET "ROOT=%~dp0"
SET "EXITCODE=0"
SET "TARGET=Build"
SET "LOGNAME=build_sppf.log"
IF /I "%~1"=="rebuild" SET "TARGET=Rebuild"
IF /I "%~1"=="rebuild" SET "LOGNAME=build_sppf_rebuild.log"
SET "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
SET "VSDIR="
IF EXIST "%VSWHERE%" FOR /F "USEBACKQ TOKENS=*" %%I IN (`"%VSWHERE%" -latest -prerelease -products * -requires Microsoft.Component.MSBuild -property installationPath`) DO SET "VSDIR=%%I"
IF DEFINED VSDIR GOTO HaveVS
IF NOT EXIST "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools" GOTO NoMSB
SET "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
:HaveVS
SET "MSB=%VSDIR%\MSBuild\Current\Bin\MSBuild.exe"
SET "YASMDIR=%TEMP%\spp2pgs_yasm"
SET "LOG=%ROOT%%LOGNAME%"
SET "SRC=%ROOT%misc\xy-VSFilter\src"
SET "SLNOUT=%SRC%\bin\lib_18.0\x64\Release"
SET "SDK=10.0.26100.0"

IF NOT EXIST "%MSB%" GOTO NoMSB
IF NOT EXIST "C:\Program Files (x86)\Windows Kits\10\include\%SDK%" GOTO NoSDK
IF NOT EXIST "%SRC%\VSFilter.sln" GOTO NoSRC

IF NOT EXIST "%YASMDIR%\yasm.exe" MKDIR "%YASMDIR%" 2>NUL
IF NOT EXIST "%YASMDIR%\yasm.exe" COPY /Y "%ROOT%misc\vsyasm\vsyasm.exe" "%YASMDIR%\yasm.exe" >NUL
IF NOT EXIST "%YASMDIR%\yasm.exe" ECHO Warning: yasm.exe not prepared.

CALL "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >NUL
WHERE cl.exe >NUL 2>&1
IF ERRORLEVEL 1 GOTO NoVCENV
SET "PATH=%VSDIR%\MSBuild\Current\Bin;%YASMDIR%;%PATH%"
SET "VisualStudioVersion=18.0"

FOR /F "TOKENS=2 DELIMS=:" %%A IN ('CHCP') DO SET "OLDCP=%%A"
SET "OLDCP=%OLDCP: =%"
CHCP 65001 >NUL
SET "VSLANG=1033"

ECHO Visual Studio: %VSDIR%
ECHO Building VSFilter.sln (Release x64, toolset v145, SDK %SDK%)
ECHO Then copying VSSppf.dll to bin as xy-VSSppf64.dll
ECHO Target: %TARGET%, log: %LOGNAME%
ECHO This build is much heavier than the core one, please wait ...
ECHO.
"%MSB%" "%SRC%\VSFilter.sln" /nologo /m /t:%TARGET% /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VisualStudioVersion=18.0 /p:WindowsTargetPlatformVersion=%SDK% /verbosity:minimal "/flp:LogFile=%LOG%;Encoding=UTF-8;Verbosity=minimal"
SET "RC=%ERRORLEVEL%"
ECHO.
IF NOT "%RC%"=="0" GOTO Failed

IF NOT EXIST "%SLNOUT%\VSSppf.dll" GOTO NoDLL
IF NOT EXIST "%ROOT%bin" MKDIR "%ROOT%bin"
COPY /Y "%SLNOUT%\VSSppf.dll" "%ROOT%bin\xy-VSSppf64.dll" >NUL
IF NOT EXIST "%SLNOUT%\VSSppf.pdb" GOTO SkipPdb
COPY /Y "%SLNOUT%\VSSppf.pdb" "%ROOT%bin\xy-VSSppf64.pdb" >NUL
:SkipPdb
IF NOT EXIST "%ROOT%bin\xy-VSSppf64.dll" GOTO NoDLL
ECHO BUILD OK, bin\xy-VSSppf64.dll is ready.
GOTO Done
:NoSRC
ECHO Submodule missing, %SRC%\VSFilter.sln not found.
ECHO Run: git submodule update --init --recursive
SET "EXITCODE=1"
GOTO Done
:NoDLL
ECHO BUILD finished but VSSppf.dll was not produced.
SET "EXITCODE=1"
GOTO Done
:Failed
ECHO BUILD FAILED, full log: %LOG%
SET "EXITCODE=1"
GOTO Done
:NoMSB
ECHO MSBuild not found under "%VSDIR%".
SET "EXITCODE=1"
GOTO Done
:NoSDK
ECHO Windows SDK %SDK% not found.
SET "EXITCODE=1"
GOTO Done
:NoVCENV
ECHO VC environment not ready: cl.exe not found after vcvars64.bat
SET "EXITCODE=1"
:Done
IF DEFINED OLDCP CHCP %OLDCP% >NUL
IF DEFINED CI EXIT /B %EXITCODE%
cmd /k
