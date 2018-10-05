
# If your compiler cannot find asio, please specify it explicitly like this:
#asio_dir = -I/path of asio/
# asio.hpp and asio directory should be available in this place.

# If possible, -std=c++14 or -std=c++1y would be better.
cflag = -Wall -fexceptions -std=c++11
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DASIO_STANDALONE -DASIO_NO_DEPRECATED

kernel = ${shell uname -s}
ifeq (${kernel}, SunOS)
	cflag += -pthreads
	lflag += -pthreads -lsocket -lnsl
else
	cflag += -pthread
	lflag += -pthread

	cygwin = ${findstring CYGWIN, ${kernel}}
	ifeq (${cygwin}, CYGWIN)
		cflag += -D__USE_W32_SOCKETS -D_WIN32_WINNT=0x0501
		lflag += -lws2_32 -lwsock32
	endif
endif

cflag += ${ext_cflag} ${asio_dir} -I../../include/
lflag += ${ext_libs}

target = ${dir}/${module}
sources = ${shell ls *.cpp}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell mkdir -p ${dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${subst .cpp,.d,${dir}/$<} $< 1>/dev/null
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	-rm -rf debug release

