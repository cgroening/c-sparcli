CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude

SRC     = src/style.c src/panel.c src/color.c src/text.c src/table.c src/columns.c src/rule.c src/tree.c src/list.c src/progressbar.c src/spinner.c src/kv.c src/alert.c src/badge.c src/util.c src/pad.c src/markup.c
OBJ     = $(SRC:.c=.o)
LIB     = libsparcli.a

TEST_SRC = tests/test_main.c
TEST_BIN = tests/test_main

.PHONY: all test clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) -L. -lsparcli -o $(TEST_BIN)
	./$(TEST_BIN)

clean:
	rm -f $(OBJ) $(LIB) $(TEST_BIN)
