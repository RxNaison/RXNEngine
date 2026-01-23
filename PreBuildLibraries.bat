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
cmake [-G generator] [-DYAML_BUILD_SHARED_LIBS=OFF] ..
cmake --build . --config Release
cmake --build . --config Debug
cd ..