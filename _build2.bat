call "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64 >nul 2>&1
cl.exe /std:c++17 /O2 /W3 /EHsc /DNDEBUG /D_CRT_SECURE_NO_WARNINGS ^
    /I"C:\Users\barbo\SDL2\SDL2-2.30.12\include" ^
    /I"src\core" /I"src\world" /I"src\rendering" /I"src\utils" /I"src\time" /I"src\weather" /I"src\craft" /I"src\ui" /I"src\physics" ^
    src\main.cpp src\core\renderer.cpp src\rendering\sky.cpp ^
    src\rendering\texture_atlas.cpp src\rendering\debug_overlay.cpp ^
    src\rendering\weather_particles.cpp ^
    src\world\chunk.cpp src\world\chunk_manager.cpp ^
    /Fe:voxel_engine.exe ^
    /link /SUBSYSTEM:CONSOLE /LIBPATH:"C:\Users\barbo\SDL2\SDL2-2.30.12\lib\x64" ^
    SDL2main.lib SDL2.lib opengl32.lib user32.lib gdi32.lib kernel32.lib shell32.lib