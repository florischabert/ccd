CFLAGS+=-O3 -Wall -Wextra
LDFLAGS+=-lusb-1.0

all: ccd


clean:
	$(RM) -rf ccd *.o