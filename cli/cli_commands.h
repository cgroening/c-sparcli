/**
 * @file cli_commands.h
 * Declarations of every subcommand implementation. The dispatch table in
 * `main.c` maps command names to these functions.
 *
 * Convention: each command receives the global context plus its own argv
 * slice (argv[0] = command name) and returns a `SC_CLI_EXIT_*` code.
 */
#pragma once

#include "cli_common.h"

/* ── Output commands (cmd_output.c) ────────────────────────────────────── */
int sc_cli_cmd_print(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_panel(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_rule(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_list(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_kv(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_alert(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_badge(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_columns(ScCliCtx *ctx, int argc, char **argv);

/* ── Table command (cmd_table.c) ───────────────────────────────────────── */
int sc_cli_cmd_table(ScCliCtx *ctx, int argc, char **argv);

/* ── Tree command (cmd_tree.c) ─────────────────────────────────────────── */
int sc_cli_cmd_tree(ScCliCtx *ctx, int argc, char **argv);

/* ── Streaming commands (cmd_progress.c) ───────────────────────────────── */
int sc_cli_cmd_spin(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_progress(ScCliCtx *ctx, int argc, char **argv);

/* ── Input commands (cmd_input.c) ──────────────────────────────────────── */
int sc_cli_cmd_confirm(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_input(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_password(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_number(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_textarea(ScCliCtx *ctx, int argc, char **argv);

/* ── Selection input commands (cmd_select.c) ───────────────────────────── */
int sc_cli_cmd_select(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_fuzzy(ScCliCtx *ctx, int argc, char **argv);
int sc_cli_cmd_date(ScCliCtx *ctx, int argc, char **argv);
