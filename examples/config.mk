
# If your compiler cannot find boost, please specify it explicitly like this:
#boost_include_dir = -I/usr/local/include/
#boost_lib_dir = -I/usr/local/lib/

# If possible, a higher edition of c++ would be always better.
cflag = -Wall -fexceptions -std=c++11
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DBOOST_ASIO_NO_DEPRECATED -DBOOST_CHRONO_HEADER_ONLY

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
	ignore = 1>nul
	make_dir = md ${dir} 2>nul
	del_dirs = -rd /S /Q debug release 2>nul
else
	ignore = 1>/dev/null
	make_dir = mkdir -p ${dir}
	del_dirs = -rm -rf debug release
endif

cflag += ${ext_cflag} ${boost_include_dir} -I../../include/
lflag += ${ext_libs} ${boost_lib_dir}

target = ${dir}/${module}
objects = ${patsubst %.cpp,${dir}/%.o,${wildcard *.cpp}}
deps = ${patsubst %.o,%.d,${objects}}
${shell ${make_dir}}

release debug : ${target}
-include ${deps}
${target} : ${objects}
	${CXX} -o $@ $^ ${lflag}
${objects} : ${dir}/%.o : %.cpp
	${CXX} ${cflag} -E -MMD -w -MT '$@' -MF ${patsubst %.cpp,%.d,${dir}/$<} $< ${ignore}
	${CXX} ${cflag} -c $< -o $@

.PHONY : clean
clean:
	${del_dirs}
