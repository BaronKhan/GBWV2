@echo off
echo Building GameBoy Emulator...

if not exist build mkdir build
cd build

echo Configuring CMake...
cmake -G "Visual Studio 17 2022" -A x64 ..

echo Building project...
cmake --build . --config Debug

echo Done!
cd ..

echo You can now run the emulator from build\bin\Debug\GameBoyEmulator.exe 