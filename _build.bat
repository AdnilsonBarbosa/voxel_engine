call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cl.exe /std:c++17 /O2 /W3 /EHsc /DNDEBUG /I"C:\Users\barbo\SDL2\SDL2-2.30.12\include" test_render.cpp /Fe:test_render.exe /link /SUBSYSTEM:CONSOLE /LIBPATH:"C:\Users\barbo\SDL2\SDL2-2.30.12\lib\x64" SDL2main.lib SDL2.lib opengl32.lib user32.lib gdi32.lib kernel32.lib shell32.lib
