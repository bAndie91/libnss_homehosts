
set -e

gcc -g -O2 -Wall -Wpointer-arith -fPIC -c \
	-o libnss_homehosts.o libnss_homehosts.c "$@"
gcc -g -O2 -Wall -Wpointer-arith -shared -Wl,-soname,libnss_homehosts.so.2 \
	-Wl,-z,defs -o libnss_homehosts.so.2 libnss_homehosts.o
