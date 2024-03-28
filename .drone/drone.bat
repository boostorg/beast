
@ECHO ON
setlocal enabledelayedexpansion

if "%DRONE_JOB_BUILDTYPE%" == "boost" (

echo "============> INSTALL"

SET DRONE_BUILD_DIR=%CD: =%
choco install --no-progress -y openssl --x64 --version 1.1.1.1000
mklink /D "C:\OpenSSL" "C:\Program Files\OpenSSL-Win64"
SET OPENSSL_ROOT=C:\OpenSSL
SET BOOST_BRANCH=develop
IF "%DRONE_BRANCH%" == "master" SET BOOST_BRANCH=master
echo using openssl : : ^<include^>"C:/OpenSSL/include" ^<search^>"C:/OpenSSL/lib" ^<ssl-name^>libssl ^<crypto-name^>libcrypto : ^<address-model^>64 ; >> %USERPROFILE%\user-config.jam
cd ..
SET GET_BOOST=!DRONE_BUILD_DIR!\tools\get-boost.sh
bash -c "$GET_BOOST $DRONE_BRANCH $DRONE_BUILD_DIR"
cd boost-root
call bootstrap.bat
b2 headers

echo "============> SCRIPT"
cd libs/beast

IF DEFINED DEFINE SET B2_DEFINE="define=%DEFINE%"

echo "Running tests"
..\..\b2 --debug-configuration variant=%VARIANT% cxxstd=%CXXSTD% %B2_DEFINE% address-model=%ADDRESS_MODEL% toolset=%TOOLSET% --verbose-test -j3 test
if !errorlevel! neq 0 exit /b !errorlevel!

echo "Running libs/beast/example"
..\..\b2 --debug-configuration variant=%VARIANT% cxxstd=%CXXSTD% %B2_DEFINE% address-model=%ADDRESS_MODEL% toolset=%TOOLSET% -j3 example
if !errorlevel! neq 0 exit /b !errorlevel!

echo "Running run-fat-tests"
..\..\b2 --debug-configuration variant=%VARIANT% cxxstd=%CXXSTD% %B2_DEFINE% address-model=%ADDRESS_MODEL% toolset=%TOOLSET% --verbose-test test//run-fat-tests -j3
if !errorlevel! neq 0 exit /b !errorlevel!

echo "============> COMPLETED"

)
