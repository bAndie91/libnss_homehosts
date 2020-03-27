CFLAGS:=-O2 -Wall -Werror -Wpointer-arith -fPIC $(CFLAGS)
VERSION:=2
LIB_NAME:=libnss_envhosts.so

.PHONY: all
all: $(LIB_NAME)

$(LIB_NAME): libnss_envhosts.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,$(LIB_NAME).$(VERSION) -o $@ $<

libnss_envhosts.o: src/libnss_envhosts.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(LIB_NAME) libnss_envhosts.o
