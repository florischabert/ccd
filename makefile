CFLAGS+=-O3 -Wall
LDFLAGS+=-lusb

all: ccd


clean:
	$(RM) -rf ccd *.o