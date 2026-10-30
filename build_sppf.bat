@ECHO OFF
SETLOCAL
SET "ROOT=%~dp0"
SET "VSDIR=C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools"
SET "VSBIN=%VSDIR%\MSBuild\Current\Bin"
SET "MSB=%VSBIN%\MSBuild.exe"
SET "YASMDIR=%TEMP%\spp2pgs_yasm"
SET "LOG=%ROOT%build_sppf.log"
SET "SRC=%ROOT%misc\xy-VSFilter\src"
SET "SLNOUT=%SRC%\bin\lib_18.0\x64\Release"
SET "SDK=10.0.26100.0"

IF NOT EXIST "%MSB%" GOTO NoMSB
IF NOT EXIST "C:\Program Files (x86)\Windows Kits\10\include\%SDK%" GOTO NoSDK

IF NOT EXIST "%YASMDIR%\yasm.exe" MKDIR "%YASMDIR%" 2>NUL
IF NOT EXIST "%YASMDIR%\yasm.exe" COPY /Y "%ROOT%misc\vsyasm\vsyasm.exe" "%YASMDIR%\yasm.exe" >NUL
IF NOT EXIST "%YASMDIR%\yasm.exe" ECHO Warning: yasm.exe not prepared.

CALL "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >NUL
WHERE cl.exe >NUL 2>&1
IF ERRORLEVEL 1 GOTO NoVCENV
SET "PATH=%VSBIN%;%YASMDIR%;%PATH%"
SET "VisualStudioVersion=18.0"

FOR /F "TOKENS=2 DELIMS=:" %%A IN ('CHCP') DO SET "OLDCP=%%A"
SET "OLDCP=%OLDCP: =%"
CHCP 65001 >NUL
SET "VSLANG=1033"

ECHO Building VSFilter.sln (Release x64, toolset v145, SDK %SDK%)
ECHO Then copying VSSppf.dll to bin as xy-VSSppf64.dll
ECHO This build is much heavier than the core one, please wait ...
ECHO.
"%MSB%" "%SRC%\VSFilter.sln" /nologo /m /t:Build /p:Configuration=Release /p:Platform=x64 /p:PlatformToolset=v145 /p:VisualStudioVersion=18.0 /p:WindowsTargetPlatformVersion=%SDK% /verbosity:minimal "/flp:LogFile=%LOG%;Encoding=UTF-8;Verbosity:minimal"
SET "RC=%ERRORLEVEL%"
ECHO.
IF NOT "%RC%"=="0" GOTO Failed

IF NOT EXIST "%SLNOUT%\VSSppf.dll" GOTO NoDLL
COPY /Y "%SLNOUT%\VSSppf.dll" "%ROOT%bin\xy-VSSppf64.dll" >NUL
IF NOT EXIST "%SLNOUT%\VSSppf.pdb" GOTO SkipPdb
COPY /Y "%SLNOUT%\VSSppf.pdb" "%ROOT%bin\xy-VSSppf64.pdb" >NUL
:SkipPdb
IF NOT EXIST "%ROOT%bin\xy-VSSppf64.dll" GOTO NoDLL
ECHO BUILD OK, bin\xy-VSSppf64.dll is ready.
GOTO Done
:NoDLL
ECHO BUILD finished but VSSppf.dll was not produced.
GOTO Done
:Failed
ECHO BUILD FAILED, full log: %LOG%
GOTO Done
:NoMSB
ECHO MSBuild not found: %MSB%
GOTO Done
:NoSDK
ECHO Windows SDK %SDK% not found.
GOTO Done
:NoVCENV
ECHO VC environment not ready: cl.exe not found after vcvars64.bat
:Done
IF DEFINED OLDCP CHCP %OLDCP% >NUL
cmd /k
