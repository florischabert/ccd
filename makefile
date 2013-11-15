SRC=$(wildcard src/*.c)
OBJ=$(SRC:.c=.o)
DEP=$(SRC:.c=.deps)
BIN=ccd

CFLAGS+=-Wall -Wextra -O3
LDFLAGS+=-lusb-1.0

%.o: %.c
	@echo CC $@
	@$(CC) $(CFLAGS) -MD -MF $(@:.o=.deps) -o $@ -c $<

$(BIN): $(SRC:.c=.o)
	@echo LD $@
	@$(CC) $(LDFLAGS) -o $@ $^

-include $(DEP)
	
clean:
	@$(RM) $(BIN) $(DEP) $(OBJ)

.PHONY: clean

