
# If your compiler cannot find asio, please specify it explicitly like this:
asio_dir = -I../../asio/asio/include/
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

target_machine = ${shell ${CXX} -dumpmachine}
ifneq (, ${findstring solaris, ${target_machine}})
	cflag += -pthreads
	lflag += -pthreads -lsocket -lnsl
else
	cflag += -pthread
	lflag += -pthread

	ifneq (, ${findstring cygwin, ${target_machine}})
		cflag += -D__USE_W32_SOCKETS -D_WIN32_WINNT=0x0501
		lflag += -lws2_32 -lwsock32
	endif
endif

ifneq (, ${findstring mingw, ${target_machine}})
	SHELL = cmd
	cflag += -D__USE_MINGW_ANSI_STDIO=1
	lflag += -lws2_32 -lwsock32
	sources = ${shell dir /B *.cpp}
	ignore = 1>nul
	make_dir = md ${dir} 2>nul
	del_dirs = -rd /S /Q debug release 2>nul
else
	sources = ${shell ls *.cpp}
	ignore = 1>/dev/null
	make_dir = mkdir -p ${dir}
	del_dirs = -rm -rf debug release
endif

cflag += ${ext_cflag} ${asio_dir} -I../../include/
lflag += ${ext_libs}

target = ${dir}/${module}
objects = ${patsubst %.cpp,${dir}/%.o,${sources}}
deps = ${patsubst %.o,%.d,${objects}}
${shell ${make_dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${subst .cpp,.d,${dir}/$<} $< ${ignore}
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	${del_dirs}
