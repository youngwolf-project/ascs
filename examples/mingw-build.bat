@echo off
set cflag=-Wall -fexceptions -std=c++17 -pthread -O2 -DNDEBUG -DASIO_STANDALONE -DASIO_NO_DEPRECATED -I../../../asio/asio/include -I../../include/
set lflag=-pthread -s -lstdc++ -lws2_32 -lwsock32

for /d %%i in (*) do call:build %%i
goto:eof

:build
@echo on
cd %~1
g++ %cflag% *.cpp -o %~1.exe %lflag%
cd ..
@echo off
goto:eof
