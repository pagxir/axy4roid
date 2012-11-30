OBJECTS = pstcp_http.o
SOCKS_OBJ = tcp_listen.o proxy5.o tcp_socks.o

LDFLAGS = -Llibwait
CXXFLAGS = -Ilibwait/include -D_HTTP_SERVER_ -g 
LDLIBS = -lrt -lstdc++ -lwait

proxy5: $(SOCKS_OBJ) $(OBJECTS) libwait/libwait.a

libwait/libwait.a:
	$(MAKE) -C libwait

clean:
	$(RM) -f $(SOCKS_OBJ) $(OBJECTS)
