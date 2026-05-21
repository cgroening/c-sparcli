CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude

SRC     = src/style.c src/print.c src/panel.c src/color.c src/text.c src/table.c src/columns.c src/rule.c src/tree.c src/list.c src/progressbar.c src/spinner.c src/kv.c src/alert.c src/badge.c src/util.c src/pad.c src/markup.c
OBJ     = $(SRC:.c=.o)
LIB     = libsparcli.a

TEST_SRC = tests/test_main.c \
           tests/test_styles.c \
           tests/test_colors.c \
           tests/test_panels.c \
           tests/test_tables.c \
           tests/test_columns.c \
           tests/test_rules.c \
           tests/test_trees.c \
           tests/test_lists.c \
           tests/test_progressbar.c \
           tests/test_spinner.c \
           tests/test_kv.c \
           tests/test_alert.c \
           tests/test_badge.c \
           tests/test_util.c \
           tests/test_pad.c \
           tests/test_align.c \
           tests/test_markup.c
TEST_BIN = tests/test_main

.PHONY: all test clean

all: $(LIB)

$(LIB): $(OBJ)
	ar rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) -L. -lsparcli -o $(TEST_BIN)
	./$(TEST_BIN) $(ARGS)

clean:
	rm -f $(OBJ) $(LIB) $(TEST_BIN)
