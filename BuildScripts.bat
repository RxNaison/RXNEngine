cd RXNScriptHost

dotnet build RXNScriptHost.csproj -c Debug
cd ..

cd RXNScriptCore
dotnet build RXNScriptCore.csproj -c Debug

PAUSE