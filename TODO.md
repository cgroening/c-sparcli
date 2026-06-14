# TODO

## Quality

Potential quality-improvement measures (the test suite itself is already strong:
golden files, PTY suites, ASan/UBSan/TSan, fuzzing, completion-parity, FFI-poison
gate). These are about automation, measurement, and project hygiene.

- [ ] **Coverage target (`make coverage`).** Clang source-based coverage
  (`-fprofile-instr-generate -fcoverage-mapping`) over the headless C test
  runners (output `--no-animated`, input `--logic`, style, app, args, serde,
  view) in a dedicated `build.coverage.nosync` tree – mirror the
  sanitizer/tsan build-tree pattern. Merge with `llvm-profdata merge`, report
  with `llvm-cov report` (ignore `tests/|examples/|cli/`); optional
  `make coverage-html` via `llvm-cov show -format=html`. Make it tool-optional
  (skip with an install hint when `llvm-cov`/`llvm-profdata` are absent, like
  `make lint`). Wire into `.PHONY`, `clean`, and the `-include` deps.

- [ ] **Format gate (`make format` / `make format-check`).** Add a
  `.clang-format` matching the existing style (LLVM base + `IndentWidth: 4`,
  `ColumnLimit: 80`, `PointerAlignment: Right`, `AlignAfterOpenBracket:
  BlockIndent`, attach braces, short braced blocks on one line) and an
  `.editorconfig`. `make format` applies `clang-format -i`; `make format-check`
  runs `clang-format --dry-run -Werror` and fails on diff. Both tool-optional
  (install hint when `clang-format` is missing). Tune the config so the gate
  starts green (clang-format is not currently installed locally, so the drift
  could not be measured yet). Keep it standalone for now – do **not** wire into
  `make qa` until the config is proven stable.

- [ ] **Hygiene files.** `LICENSE` already exists (MIT). Still missing:
  - [ ] `CONTRIBUTING.md` – point to `docs/development.md` for the workflow,
    list the gate ladder (`make test` → `make qa`, plus opt-in `make qa-serde` /
    `make qa-view`, the golden-regen targets, and the new `make format-check` /
    `make coverage`), code style (`CLAUDE.md` conventions, 80-col code), and the
    conventional-commit message style.
  - [ ] `SECURITY.md` – supported versions (pre-1.0: latest only), how to report
    (GitHub private security advisory, email fallback), and the project's core
    security property: the ANSI sanitization / trust boundary (untrusted strings
    are sanitized at the library edge; `sc_set_allow_ansi` opt-out).
  - [ ] `CHANGELOG.md` – placeholder only: "Keep a Changelog" header plus a note
    that the changelog will be maintained starting with the final v0.1.0
    release; until then, see the git history.

- [ ] **Docs.** Add the new `make coverage`, `make format`, and
  `make format-check` targets to the `CLAUDE.md` build listing and to
  `docs/development.md` (the project documents every make target).

### Later / deferred

- [ ] **CI (`.github/workflows`).** GitHub Actions matrix (Linux + macOS)
  running `make qa` + `make qa-serde` + `make qa-view` (plus `make format-check`
  and `make coverage` once they exist). Optional Linux-only `libFuzzer` job
  (Apple clang ships no libFuzzer). Biggest single habit-forming win, since it
  enforces all the gates that already exist.

- [ ] **Extend coverage instrumentation.** Bring the CLI / PTY / C++ suites
  under coverage too (they need separately instrumented binaries and forkpty
  handling), and add a gcc/`lcov` coverage path as an alternative to the
  clang/`llvm-cov` one.
