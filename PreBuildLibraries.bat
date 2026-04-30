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

cd SDL
mkdir build
cd build
cmake .. -DSDL_SHARED=ON -DSDL_STATIC=OFF 
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

cd CoACD
git submodule update --init --recursive
mkdir build
cd build
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=ON -DTBB_BUILD_SHARED=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL -DOPENVDB_CORE_SHARED=OFF -DTBB_TEST=OFF -DCMAKE_CXX_FLAGS="/MD /EHsc /openmp"
cmake --build . --config Release
cmake .. -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DBUILD_SHARED_LIBS=ON -DTBB_BUILD_SHARED=OFF -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL -DOPENVDB_CORE_SHARED=OFF -DTBB_TEST=OFF -DCMAKE_CXX_FLAGS="/MDd /EHsc /openmp"
cmake --build . --config Debug
cd ../..

echo All libraries built successfully!
pause