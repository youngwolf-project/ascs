set cflag=-Wall -fexceptions -std=c++17 -pthread -O2 -DNDEBUG -DASIO_STANDALONE -DASIO_NO_DEPRECATED -I../../../asio/asio/include -I../../include/
set lflag=-pthread -s -lstdc++ -lws2_32 -lwsock32

cd client
g++ %cflag% client.cpp -o client.exe %lflag%
cd ../echo_client
g++ %cflag% echo_client.cpp -o echo_client.exe %lflag%
cd ../echo_server
g++ %cflag% echo_server.cpp -o echo_server.exe %lflag%
cd ../concurrent_client
g++ %cflag% concurrent_client.cpp -o concurrent_client.exe %lflag%
cd ../concurrent_server
g++ %cflag% concurrent_server.cpp -o concurrent_server.exe %lflag%
cd ../pingpong_client
g++ %cflag% pingpong_client.cpp -o pingpong_client.exe %lflag%
cd ../pingpong_server
g++ %cflag% pingpong_server.cpp -o pingpong_server.exe %lflag%
cd ../file_client
g++ %cflag% file_client.cpp -o file_client.exe %lflag%
cd ../file_server
g++ %cflag% file_socket.cpp file_server.cpp -o file_server.exe %lflag%
cd ../udp_test
g++ %cflag% udp_test.cpp -o udp_test.exe %lflag%
cd ../ssl_test
g++ %cflag% ssl_test.cpp -o ssl_test.exe %lflag%
cd ..

