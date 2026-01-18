cd RXNEngine/vendor/assimp
mkdir build
cd build
cmake .. -DBUILD_SHARED_LIBS=OFF
cmake --build . --config Release
cmake --build . --config Debug
cd ../../../..

call premake5.exe vs2022
PAUSE