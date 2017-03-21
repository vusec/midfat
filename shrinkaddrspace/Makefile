CFLAGS  := -fpic -Wall -Wextra -g -O2
LDFLAGS := -shared -O2 -Wl,-rpath,$(shell python2 -c 'print 256 * "x"')
LDLIBS  := -ldl

all: libshrink-preload.so libshrink-static.a

libshrink-preload.so: libpreload.o shrink.o
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

libshrink-static.a: libstatic.o shrink.o
	ar rcs $@ $^

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

shrink.o: shrink.h

clean:
	rm *.o *.so *.a
