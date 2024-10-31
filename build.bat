@echo off
setlocal EnableDelayedExpansion

set "valid_projects=boost ftxui"
set "is_valid_project=false"
set "project=%1"

for %%p in (%valid_projects%) do (
    if "%project%"=="%%p" (
        set "is_valid_project=true"
        goto :found
    )
)

:found
if "%is_valid_project%"=="false" (
    echo Invalid Proj: %project%
    echo Possible Projs: %valid_projects%
    exit /b 1
)

echo building: %project%


if "%project%"=="boost" (
    cd third_repos/boost

    for /f "usebackq delims=" %%i in (`python -c "import sysconfig; print(sysconfig.get_paths()['include'].replace('\\', '/'))"`) do (
        set "PYTHON_INCLUDE_PATH=%%i"
    )
    for /f "usebackq delims=" %%j in (`python -c "import sysconfig; print(sysconfig.get_paths()['platlib'].replace('\\', '/'))"`) do (
        set "PYTHON_LIB_PATH=%%j"
    )
    for /f "usebackq delims=" %%k in (`python -c "import sys; print(sys.executable.replace('\\', '/'))"`) do (
        set "PYTHON_EXECUTABLE=%%k"
    )
    for /f "usebackq delims=" %%v in (`python -c "import sys; print(f'{sys.version_info[0]}.{sys.version_info[1]}'.replace('\\', '/'))"`) do (
        set "PYTHON_VERSION=%%v"
    )
    (
        echo using python : !PYTHON_VERSION! : !PYTHON_EXECUTABLE! : !PYTHON_INCLUDE_PATH! : !PYTHON_LIB_PATH! ;
    ) > tools\build\src\user-config.jam

    call bootstrap.bat clang
    b2 --with-python

    cd stage/lib
    dir /b
    mkdir "../../../lib/win/boost"
    move *.lib ../../../lib/win/boost
    cd ./../..
    xcopy "./boost" "../include/boost" /E /I

) else if "%project%"=="ftxui" (
    cd third_repos/ftxui
    mkdir build
    cd build
    cmake -G "Ninja" -DCMAKE_CXX_COMPILER=clang++ .. -DFTXUI_BUILD_EXAMPLES=OFF -DFTXUI_BUILD_DOCS=OFF -DFTXUI_BUILD_TESTS=OFF -DFTXUI_BUILD_TESTS_FUZZER=OFF -DFTXUI_ENABLE_INSTALL=OFF 
    cmake --build .
    mkdir "../../../utils/lib/win/ftxui"
    move *.a ../../../utils/lib/win/ftxui
    cd ./../include
    xcopy "./ftxui" "../../../utils/include/ftxui" /E /I
)

echo build done

endlocal
exit /b 0