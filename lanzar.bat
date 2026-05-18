@echo off
chcp 65001 >nul
echo ============================================
echo   Analizador ILD1302  ^|  Modo Automatico
echo ============================================
echo.

:: Verificar que Python existe
python --version >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Python no encontrado en el sistema.
    echo.
    echo Descarga e instala Python 3.12 desde:
    echo   https://www.python.org/downloads/
    echo.
    echo IMPORTANTE: Durante la instalacion, marca la casilla
    echo   "Add Python to PATH"
    echo.
    pause
    exit /b 1
)

:: Verificar version >= 3.9
python -c "import sys; v=sys.version_info; exit(0 if (v.major,v.minor)>=(3,9) else 1)" >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo [AVISO] Se recomienda Python 3.9 o superior.
    echo Descarga Python 3.12: https://www.python.org/downloads/
    echo.
    echo Presiona cualquier tecla para intentar continuar de todas formas...
    pause >nul
    echo.
)

echo Iniciando analizador...
echo.
python "%~dp0analizador_automatico.py"

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] El script termino con un error.
    pause
)
