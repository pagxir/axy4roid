OBJECTS = usertcp/rgnbuf.o usertcp/slotwait.o usertcp/slotsock.o usertcp/module.o usertcp/tcp_device.o \
		  usertcp/tcp_input.o usertcp/tcp_output.o usertcp/tcp_timer.o usertcp/slotipc.o \
		  usertcp/tcp_usrreq.o usertcp/callout.o usertcp/platform.o
SRV_OBJ = usertcp/server.o usertcp/pstcp_channel.o usertcp/pstcp_listen.o 
CLT_OBJ = usertcp/client.o usertcp/tcp_listen.o usertcp/tcp_channel.o
USRV_OBJ = usertcp/server.o usertcp/pstcp_listen.o usertcp/pstcp_http.o 
SOCKS_OBJ = tcp_listen.o proxy5.o tcp_socks.o


CXXFLAGS = -Iusertcp -g
LDLIBS = -lrt -lstdc++
proxy5: $(SOCKS_OBJ) $(OBJECTS)
