
# If your compiler cannot find asio, please specify it explicitly like this:
#ext_location = -I/path of asio/
# asio.hpp and asio directory should be available in this place.

# If possible, open c++17 (-std=c++17) would be better.
cflag = -Wall -fexceptions -std=c++1y
ifeq (${MAKECMDGOALS}, debug)
	cflag += -g -DDEBUG
	dir = debug
else
	cflag += -O2 -DNDEBUG
	lflag = -s
	dir = release
endif
cflag += -DASIO_STANDALONE -DASIO_HAS_STD_CHRONO
# If your compiler detected duplicated 'shared_mutex' definition, please define ASCS_HAS_STD_SHARED_MUTEX macro:
#cflag += -DASCS_HAS_STD_SHARED_MUTEX
# If you used concurrent queue (https://github.com/cameron314/concurrentqueue), please define ASCS_HAS_CONCURRENT_QUEUE macro:
#cflag += -DASCS_HAS_CONCURRENT_QUEUE
# And guarantee header file concurrentqueue.h is reachable, for example, add its path to ext_location:
#ext_location += -I/path of concurrent queue/
cflag += -pthread ${ext_cflag} ${ext_location} -I../../include/
lflag += -pthread ${ext_libs}

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

