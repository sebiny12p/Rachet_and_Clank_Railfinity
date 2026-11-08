@echo off
setlocal
cd /d "%~dp0"

set "UNITY_ANDROID=C:\Program Files\Unity\Hub\Editor\2022.1.16f1-x86_64\Editor\Data\PlaybackEngines\AndroidPlayer"
set "JAVA_HOME=%UNITY_ANDROID%\OpenJDK"
set "ANDROID_SDK=%UNITY_ANDROID%\SDK"
set "ANDROID_NDK=%UNITY_ANDROID%\NDK"
set "BUILD_TOOLS=%ANDROID_SDK%\build-tools\30.0.2"
set "PLATFORM_JAR=%ANDROID_SDK%\platforms\android-30\android.jar"
set "CMAKE_BIN=C:\Program Files\JetBrains\CLion 2026.1.3\bin\cmake\win\x64\bin"
set "NINJA_BIN=C:\Program Files\JetBrains\CLion 2026.1.3\bin\ninja\win\x64"

set "PATH=%JAVA_HOME%\bin;%CMAKE_BIN%;%NINJA_BIN%;%BUILD_TOOLS%;%PATH%"

echo [1/7] Building C++20 and Raylib Native Library (arm64-v8a)...
cmake -B build-android-arm64 -G Ninja ^
    -DCMAKE_TOOLCHAIN_FILE="%ANDROID_NDK%/build/cmake/android.toolchain.cmake" ^
    -DANDROID_ABI=arm64-v8a ^
    -DANDROID_PLATFORM=android-24 ^
    -DANDROID_STL=c++_static ^
    -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 exit /b 1

cmake --build build-android-arm64 -j 8
if errorlevel 1 exit /b 1

echo [2/7] Preparing Android staging directories...
if not exist "android\build\lib\arm64-v8a" mkdir "android\build\lib\arm64-v8a"
if not exist "android\build\obj" mkdir "android\build\obj"
if not exist "android\build\src" mkdir "android\build\src"
copy /Y "build-android-arm64\libmain.so" "android\build\lib\arm64-v8a\libmain.so" >nul

echo [3/7] Generating R.java from resources and app icons...
"%BUILD_TOOLS%\aapt.exe" package -f -m ^
    -S "android\res" ^
    -J "android\build\src" ^
    -M "android\AndroidManifest.xml" ^
    -I "%PLATFORM_JAR%"
if errorlevel 1 exit /b 1

echo [4/7] Compiling NativeLoader.java and R.java...
"%JAVA_HOME%\bin\javac.exe" -source 1.8 -target 1.8 ^
    -bootclasspath "%PLATFORM_JAR%" ^
    -classpath "%PLATFORM_JAR%" ^
    -d "android\build\obj" ^
    "android\build\src\com\sebinylabs\railfinity\R.java" ^
    "android\src\com\sebinylabs\railfinity\NativeLoader.java"
if errorlevel 1 exit /b 1

echo [5/7] Compiling Dalvik bytecode (classes.dex)...
call "%BUILD_TOOLS%\d8.bat" --release --output "android\build" ^
    --lib "%PLATFORM_JAR%" ^
    "android\build\obj\com\sebinylabs\railfinity\R.class" ^
    "android\build\obj\com\sebinylabs\railfinity\R$attr.class" ^
    "android\build\obj\com\sebinylabs\railfinity\R$drawable.class" ^
    "android\build\obj\com\sebinylabs\railfinity\NativeLoader.class"
if errorlevel 1 exit /b 1

echo [6/7] Packaging APK with resources, assets, classes.dex, and native arm64-v8a library...
"%BUILD_TOOLS%\aapt.exe" package -f ^
    -M "android\AndroidManifest.xml" ^
    -S "android\res" ^
    -A "assets" ^
    -I "%PLATFORM_JAR%" ^
    -F "android\build\RatchetAndClankRailfinity.unaligned.apk"
if errorlevel 1 exit /b 1

pushd "android\build"
"%BUILD_TOOLS%\aapt.exe" add "RatchetAndClankRailfinity.unaligned.apk" classes.dex
"%BUILD_TOOLS%\aapt.exe" add "RatchetAndClankRailfinity.unaligned.apk" lib/arm64-v8a/libmain.so
popd

echo [7/7] Zipaligning and signing RatchetAndClankRailfinity.apk...
"%BUILD_TOOLS%\zipalign.exe" -f 4 "android\build\RatchetAndClankRailfinity.unaligned.apk" "RatchetAndClankRailfinity.apk"
if errorlevel 1 exit /b 1

if not exist "android\sebinylabs.keystore" (
    "%JAVA_HOME%\bin\keytool.exe" -genkeypair -validity 10000 ^
        -dname "CN=Sebiny Labs,O=Sebiny Labs,C=RO" ^
        -keystore "android\sebinylabs.keystore" ^
        -storepass sebiny123 -keypass sebiny123 ^
        -alias sebinylabskey -keyalg RSA -keysize 2048
)

call "%BUILD_TOOLS%\apksigner.bat" sign ^
    --ks "android\sebinylabs.keystore" ^
    --ks-pass pass:sebiny123 ^
    --key-pass pass:sebiny123 ^
    --ks-key-alias sebinylabskey ^
    "RatchetAndClankRailfinity.apk"
if errorlevel 1 exit /b 1

echo ============================================================
echo SUCCESS: Built signed Android APK: RatchetAndClankRailfinity.apk
echo ============================================================
