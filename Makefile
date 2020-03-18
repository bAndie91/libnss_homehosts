CFLAGS:=-O2 -Wall -Werror -Wpointer-arith -fPIC $(CFLAGS)
INSTALL_FOLDER:=/lib/x86_64-linux-gnu
VERSION:=2
LIB_NAME:=libnss_homehosts.so
LIB_NAME_WITH_VERSION:=$(LIB_NAME).$(VERSION)

.PHONY: all
all: $(LIB_NAME_WITH_VERSION)

$(LIB_NAME_WITH_VERSION): libnss_homehosts.o
	$(CC) $(CFLAGS) -shared -o $@ $<

libnss_homehosts.o: libnss_homehosts.c
	$(CC) $(CFLAGS) -c -o $@ $<

.PHONY: clean
clean:
	rm -f $(LIB_NAME) libnss_homehosts.o

.PHONY: install
install: $(LIB_NAME)
	install -m 644 $< $(INSTALL_FOLDER)
	rm -f $(INSTALL_FOLDER)/$(LIB_NAME_WITH_VERSION)
	ln -s $(LIB_NAME) $(INSTALL_FOLDER)/$(LIB_NAME_WITH_VERSION)
	ldconfig
.PHONY: uninstall
uninstall:
	rm -f $(INSTALL_FOLDER)/$(LIB_NAME)
	rm -f $(INSTALL_FOLDER)/$(LIB_NAME_WITH_VERSION)
.PHONY: test
test:
	echo 198.18.1.1 libnss-homehost.test.example.net >> ~/.hosts
	getent -s homehosts hosts libnss-homehost.test.example.net
	sed -e '/^198.18.1.1 libnss-homehost.test.example.net$/d' -i ~/.hosts
