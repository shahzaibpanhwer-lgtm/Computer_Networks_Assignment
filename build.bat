@echo off
REM build.bat - compile every program with MinGW g++ on Windows
REM Run this from the Developer / MSYS2 terminal inside this folder.
setlocal
set FLAGS=-std=c++17 -Wall

echo Building single-file programs...
g++ %FLAGS% -o daytime_server.exe      daytime_server.cpp   -lws2_32
g++ %FLAGS% -o daytime_client.exe      daytime_client.cpp   -lws2_32
g++ %FLAGS% -o daytime_server2.exe     daytime_server2.cpp  -lws2_32
g++ %FLAGS% -o daytime_client2.exe     daytime_client2.cpp  -lws2_32
g++ %FLAGS% -o echo_server.exe         echo_server.cpp      -lws2_32
g++ %FLAGS% -o echo_client.exe         echo_client.cpp      -lws2_32
g++ %FLAGS% -o web_server.exe          web_server.cpp       -lws2_32
g++ %FLAGS% -o frame_server.exe        frame_server.cpp     -lws2_32
g++ %FLAGS% -o frame_client.exe        frame_client.cpp     -lws2_32
g++ %FLAGS% -o select_server.exe       select_server.cpp    -lws2_32

echo Building threaded programs...
g++ %FLAGS% -o thread_server.exe       thread_server.cpp    -lws2_32
g++ %FLAGS% -o chat_server.exe         chat_server.cpp      -lws2_32
g++ %FLAGS% -o chat_client.exe         chat_client.cpp      -lws2_32

echo.
echo Done. If chat_client or thread_server failed with a std::thread error,
echo your MinGW uses the win32 thread model. Install MinGW-w64 (POSIX threads).
endlocal
