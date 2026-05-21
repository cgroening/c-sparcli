#pragma once

char *sc_strip_ansi(const char *str);
char *sc_truncate  (const char *str, int max_cols, const char *ellipsis);
void  sc_clear_line(void);
