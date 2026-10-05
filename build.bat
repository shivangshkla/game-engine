@echo off

set CommonCompilerFlags=-Z7 -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -WX -W4 -wd4505 -wd4100 -wd4189 -wd4201 -MT -Oi -Gm- -EHa- -GR- -FC -nologo
set CommonLinkerFlags=-opt:ref -incremental:no

if not exist ..\..\build mkdir ..\..\build
pushd ..\..\build

REM Asset file building build
cl -Od %CommonCompilerFlags% ..\handmade\code\test_asset_builder.cpp -link %CommonLinkerFlags%

cl -O2 %CommonCompilerFlags% -LD ..\handmade\code\handmade.cpp -link %CommonLinkerFlags% -EXPORT:GameUpdateAndRender -EXPORT:GameGetSoundSamples
move handmade.dll ..\handmade\data\handmade.dll

cl -O2 %CommonCompilerFlags% ..\handmade\code\win32_handmade.cpp -link %CommonLinkerFlags%

popd
