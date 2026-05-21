#pragma once

#include "sparcli_types.h"

typedef struct {
    const char *sep;
    int         key_width;
    int         width;
    int         margin;
    int         item_gap;
    int         wrap_val;
    ScOptions   key_opts;
    ScOptions   val_opts;
} ScKVOpts;

typedef struct ScKV ScKV;

ScKV *sc_kv_new  (ScKVOpts opts);
void  sc_kv_add  (ScKV *kv, const char *key, const char *value);
void  sc_kv_print(const ScKV *kv);
void  sc_kv_free (ScKV *kv);
