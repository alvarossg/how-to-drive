@echo off
setlocal
title How to Mechanic - compilar y abrir
echo.
echo  ==========================================
echo   HOW TO MECHANIC - compilar y abrir
echo  ==========================================
echo.

rem --- 1. Buscar el proyecto (junto a este archivo o en C:\Juegos\HowToMechanic) ---
set "PROJ=%~dp0HowToMechanic.uproject"
if not exist "%PROJ%" set "PROJ=C:\Juegos\HowToMechanic\HowToMechanic.uproject"
if not exist "%PROJ%" goto :sinproyecto
for %%P in ("%PROJ%") do set "PROJDIR=%%~dpP"
echo  Proyecto: %PROJ%

rem --- 2. Buscar Unreal Engine 5.4 completo, en cualquier disco ---
set "UE="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.4" /v InstalledDirectory 2^>nul') do (
  if exist "%%B\Engine\Build\BatchFiles\Build.bat" set "UE=%%B"
)
for %%D in (C D E F G H I J K A B) do (
  if not defined UE if exist "%%D:\UE_5.4\Engine\Build\BatchFiles\Build.bat" set "UE=%%D:\UE_5.4"
  if not defined UE if exist "%%D:\Program Files\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" set "UE=%%D:\Program Files\Epic Games\UE_5.4"
  if not defined UE if exist "%%D:\Epic Games\UE_5.4\Engine\Build\BatchFiles\Build.bat" set "UE=%%D:\Epic Games\UE_5.4"
)
if not defined UE goto :sinunreal
if not exist "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" goto :sinunreal
echo  Unreal:   %UE%
echo.

rem --- 3. Compilar (usa Visual Studio 2022 y el Windows SDK) ---
set "LOG=%PROJDIR%compilacion.txt"
echo  Compilando... la primera vez puede tardar 5-20 minutos. No cierres esta ventana.
call "%UE%\Engine\Build\BatchFiles\Build.bat" HowToMechanicEditor Win64 Development -Project="%PROJ%" -WaitMutex > "%LOG%" 2>&1
if errorlevel 1 goto :fallo

rem --- 4. Abrir el editor ---
echo.
echo  [OK] Compilado. Abriendo el editor...
echo      La primera vez prepara datos, materiales y mapas: puede tardar.
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%"
goto :fin

:fallo
echo.
echo  [X] La compilacion ha fallado.
findstr /c:"Windows SDK must be installed" "%LOG%" >nul
if not errorlevel 1 goto :sinsdk
echo      Se abre ahora el archivo compilacion.txt con los errores.
echo      Copia TODO su contenido y pegaselo a Claude.
start "" notepad "%LOG%"
goto :fin

:sinsdk
echo      Motivo: falta el Windows SDK.
echo      Abre "Visual Studio Installer" - Modificar (en Visual Studio 2022) -
echo      pestana "Componentes individuales" - busca "Windows 11 SDK" -
echo      marca "Windows 11 SDK (10.0.22621.0)" - boton Modificar.
echo      Cuando termine, reinicia el PC y vuelve a abrir este archivo.
goto :fin

:sinproyecto
echo  [X] No encuentro HowToMechanic.uproject.
echo      Pon este archivo dentro de la carpeta del proyecto
echo      o descomprime el proyecto en C:\Juegos\HowToMechanic
goto :fin

:sinunreal
echo  [X] No encuentro una instalacion completa de Unreal Engine 5.4.
echo      Epic Games Launcher - Unreal Engine - Biblioteca - 5.4:
echo      flechita junto a Iniciar - Verificar. Luego vuelve a abrir este archivo.
goto :fin

:fin
echo.
pause
