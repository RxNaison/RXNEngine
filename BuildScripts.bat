cd RXNScriptHost

dotnet build RXNScriptHost.csproj -c Debug -o ../RXNEditor/res/scripts
cd ..

cd RXNScriptCore
dotnet build RXNScriptCore.csproj -c Debug -o ../RXNEditor/res/scripts

PAUSE