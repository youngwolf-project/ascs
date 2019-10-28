@echo off
set include=-I../../../asio/asio/include -I../../include/
set cflag=-Wall -fexceptions -std=c++11 -pthread -O2 -DNDEBUG -DASIO_STANDALONE -DASIO_NO_DEPRECATED -D__USE_MINGW_ANSI_STDIO=1
set large_file=-D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE
set want_send_notify=-DASCS_WANT_MSG_SEND_NOTIFY
set lflag=-pthread -s -lstdc++ -lws2_32 -lwsock32

for /d %%i in (*) do call:build %%i
goto:eof

:build
if "%~1" == "file_server" (
	call:do_build %~1 "%cflag% %large_file% %want_send_notify%"
) else if "%~1" == "file_client" (
	call:do_build %~1 "%cflag% %large_file%"
) else (
	call:do_build %~1 "%cflag%"
)
goto:eof

:do_build
@echo on
cd %~1
g++ %include% %~2 *.cpp -o %~1.exe %lflag%
cd ..
@echo off
goto:eof
