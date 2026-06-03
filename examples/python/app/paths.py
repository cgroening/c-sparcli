#!/usr/bin/env python3
"""Per-application XDG directories and files.

    make run-example EX=python/app/paths
"""

from __future__ import annotations

import sparcli as sc

APP_NAME = 'sparcli-paths-example'


def main() -> None:
    # Each returns a pathlib.Path; the dirs are created (0700) on first use,
    # and a failure raises sc.SparcliError.
    dirs = sc.Kv(sc.KvOpts(key_style=sc.Style.bold(sc.Color.CYAN)))
    dirs.add('Config', str(sc.config_dir(APP_NAME)))
    dirs.add('Data', str(sc.data_dir(APP_NAME)))
    dirs.add('Cache', str(sc.cache_dir(APP_NAME)))
    dirs.add('State', str(sc.state_dir(APP_NAME)))
    dirs.print()
    sc.println('')

    # A file path inside an app dir: parents created, the file is not.
    log_file = sc.app_file(sc.PathKind.STATE, APP_NAME, 'logs/run.log')
    sc.println(f'Log file would go to: {log_file}')


if __name__ == '__main__':
    main()
