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
if not exist "%PROJ%" (
  echo  [X] No encuentro HowToMechanic.uproject.
  echo      Pon este archivo dentro de la carpeta del proyecto
  echo      o descomprime el proyecto en C:\Juegos\HowToMechanic
  goto :incompleto
echo  [X] Unreal Engine 5.4 esta instalado a medias: faltan archivos del motor.
echo      Epic Games Launcher - Unreal Engine - Biblioteca:
echo      - si el recuadro de 5.4 muestra una barra de progreso, espera a que acabe;
echo      - si no, pulsa la flechita junto a Iniciar y elige Verificar.
echo      Cuando termine, vuelve a hacer doble clic en este archivo.
goto :fin

:fin
)
for %%P in ("%PROJ%") do set "PROJDIR=%%~dpP"
echo  Proyecto: %PROJ%

rem --- 2. Buscar Unreal Engine 5.4 ---
set "UE="
for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.4" /v InstalledDirectory 2^>nul') do set "UE=%%B"
if not defined UE if exist "C:\Program Files\Epic Games\UE_5.4\Engine" set "UE=C:\Program Files\Epic Games\UE_5.4"
if not defined UE (
  echo  [X] No encuentro Unreal Engine 5.4.
  echo      Instalalo desde el Epic Games Launcher.
  goto :fin
)
echo  Unreal:   %UE%
echo.
if not exist "%UE%\Engine\Build\BatchFiles\Build.bat" goto :incompleto
if not exist "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" goto :incompleto

rem --- 3. Compilar (usa Visual Studio 2022 por debajo) ---
set "LOG=%PROJDIR%compilacion.txt"
echo  Compilando... la primera vez puede tardar 5-20 minutos. No cierres esta ventana.
call "%UE%\Engine\Build\BatchFiles\Build.bat" HowToMechanicEditor Win64 Development -Project="%PROJ%" -WaitMutex > "%LOG%" 2>&1
if errorlevel 1 (
  echo.
  echo  [X] La compilacion ha fallado.
  echo      Se abre ahora el archivo compilacion.txt con los errores.
  echo      Copia TODO su contenido y pegaselo a Claude.
  start "" notepad "%LOG%"
  goto :fin
)

rem --- 4. Abrir el editor ---
echo.
echo  [OK] Compilado. Abriendo el editor...
echo      La primera vez prepara datos, materiales y mapas: puede tardar.
start "" "%UE%\Engine\Binaries\Win64\UnrealEditor.exe" "%PROJ%"

:fin
echo.
pause
