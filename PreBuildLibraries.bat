@echo off
cd RXNEngine/vendor

cd assimp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
cmake --build . --config Debug
cd ../..

cd yaml-cpp
mkdir build
cd build
cmake -DYAML_BUILD_SHARED_LIBS=OFF ..
cmake --build . --config Release
cmake --build . --config Debug
cd ../..

cd PhysX/physx

set "PRESET_FILE=buildtools\presets\public\vc17win64-cpu-only.xml"
powershell -Command "(Get-Content '%PRESET_FILE%') -replace 'name=\"NV_USE_STATIC_WINCRT\" value=\"True\"', 'name=\"NV_USE_STATIC_WINCRT\" value=\"False\"' | Set-Content '%PRESET_FILE%'"

call generate_projects.bat vc17win64-cpu-only
cd compiler/vc17win64-cpu-only

cmake --build . --config debug
cmake --build . --config checked
cmake --build . --config release
cd ../../../..

echo All libraries built successfully!
pause