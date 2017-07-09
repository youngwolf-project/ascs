
# If your compiler cannot find asio, please specify it explicitly like this:
#ext_location = -I/path of asio/
# asio.hpp and asio directory should be available in this place.

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

# If you used concurrent queue (https://github.com/cameron314/concurrentqueue), please define ASCS_HAS_CONCURRENT_QUEUE macro:
#cflag += -DASCS_HAS_CONCURRENT_QUEUE
# And guarantee header file concurrentqueue.h is reachable, for example, add its path to ext_location:
#ext_location += -I/path of concurrent queue/

kernel = ${shell uname -s}
ifeq (${kernel}, SunOS)
cflag += -pthreads ${ext_cflag} ${ext_location} -I../../include/
lflag += -pthreads -lsocket -lnsl ${ext_libs}
else
cflag += -pthread ${ext_cflag} ${ext_location} -I../../include/
lflag += -pthread ${ext_libs}
endif

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

