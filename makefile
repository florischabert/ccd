SRC=$(wildcard src/*.c)
BIN=ccd

CFLAGS+=-O3 -Wall -Wextra
LDFLAGS+=-lusb-1.0

%.o: %.c
	@echo CC $@
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -o $@ -c $<

$(BIN): $(SRC:.c=.o)
	@echo LD $@
	@$(CC) $(LDFLAGS) -o $@ $^

-include $(SRC:.c=.deps)
	
clean:
	@$(RM) $(BIN) $(SRC:.c=.o) $(SRC:.c=.deps)

.PHONY: clean

