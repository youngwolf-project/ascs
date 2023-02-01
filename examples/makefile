
ASCS_MAKE = ${MAKE_COMMAND}
ifeq (${MAKECMDGOALS}, debug)
	ASCS_MAKE += debug
else
	ifeq (${MAKECMDGOALS}, clean)
		ASCS_MAKE += clean
	endif
endif

target_machine = ${shell ${CXX} -dumpmachine}

release debug clean :
	cd echo_server && ${ASCS_MAKE}
	cd echo_client && ${ASCS_MAKE}
	cd client && ${ASCS_MAKE}
	cd file_server && ${ASCS_MAKE}
	cd file_client && ${ASCS_MAKE}
	cd pingpong_server && ${ASCS_MAKE}
	cd pingpong_client && ${ASCS_MAKE}
	cd concurrent_server && ${ASCS_MAKE}
	cd concurrent_client && ${ASCS_MAKE}
	cd socket_management && ${ASCS_MAKE}
	cd debug_assistant && ${ASCS_MAKE}
	cd udp_test && ${ASCS_MAKE}
	cd ssl_test && ${ASCS_MAKE}
	cd websocket_test && ${ASCS_MAKE}
	cd ssl_websocket_test && ${ASCS_MAKE}
ifeq (, ${findstring cygwin, ${target_machine}})
ifeq (, ${findstring mingw, ${target_machine}})
	cd unix_socket && ${ASCS_MAKE}
	cd unix_udp_test && ${ASCS_MAKE}
endif
endif

