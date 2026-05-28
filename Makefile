CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Iinclude -Isrc

SRC     = src/text_attributes.c src/print.c src/panel.c src/color.c src/text.c \
          src/table/table.c \
          src/table/table_print.c \
          src/table/table_print_init.c \
          src/table/table_print_render.c \
          src/table/table_print_render_cell.c \
          src/table/table_print_render_border.c \
          src/table/table_print_render_row.c \
          src/columns.c src/rule.c src/tree.c src/list.c src/progressbar.c src/spinner.c src/kv.c src/alert.c src/badge.c src/util.c src/pad.c src/markup.c
BUILDDIR = build.nosync
OBJ     = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))
LIB     = libsparcli.a

TEST_SRC = tests/test_main.c \
           tests/test_text_attributes.c \
           tests/test_colors.c \
           tests/test_columns_basic.c \
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

$(BUILDDIR)/%.o: src/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

test: $(LIB)
	$(CC) $(CFLAGS) $(TEST_SRC) -L. -lsparcli -o $(TEST_BIN)
	./$(TEST_BIN) $(ARGS)

clean:
	rm -rf $(BUILDDIR) $(LIB) $(TEST_BIN)
