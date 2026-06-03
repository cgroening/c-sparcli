/*
 * paths.c - per-application XDG directories and files.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/paths
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


/** Application name; becomes the directory name under each XDG base dir. */
#define APP_NAME "sparcli-paths-example"


int main(void) {
    // One directory per kind; created (mode 0700) on first use.
    char *config_dir = sc_path_config(APP_NAME);
    char *data_dir   = sc_path_data(APP_NAME);
    char *cache_dir  = sc_path_cache(APP_NAME);
    char *state_dir  = sc_path_state(APP_NAME);

    if (!config_dir || !data_dir || !cache_dir || !state_dir) {
        sc_alert_error("Could not resolve the XDG directories.");
        free(config_dir);
        free(data_dir);
        free(cache_dir);
        free(state_dir);
        return 1;
    }

    ScKV *dirs = sc_kv_new((ScKVOpts){
        .key_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                       SC_ANSI_COLOR_NONE },
    });
    sc_kv_add(dirs, "Config", config_dir);
    sc_kv_add(dirs, "Data",   data_dir);
    sc_kv_add(dirs, "Cache",  cache_dir);
    sc_kv_add(dirs, "State",  state_dir);
    sc_kv_print(dirs);
    sc_kv_free(dirs);

    free(config_dir);
    free(data_dir);
    free(cache_dir);
    free(state_dir);
    printf("\n");

    // File path inside an app dir; parent directories are created, the
    // file itself is not. Subdirectories are allowed, ".." is rejected.
    char *log_file = sc_path_file(SC_PATH_STATE, APP_NAME, "logs/run.log");
    if (log_file) {
        printf("Log file would go to: %s\n", log_file);
        free(log_file);
    }

    return 0;
}
