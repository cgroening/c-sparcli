"""Put the in-place-built package on the path so ``pytest`` needs no install."""

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))

# The suite exercises the prompts' "no TTY" error path. pytest only redirects
# stdin/stdout; the controlling terminal stays reachable via /dev/tty, so an
# interactive `pytest` run would let prompts grab the real keyboard. The
# override makes the library report "no terminal" regardless of how the
# suite is started (`make python-test` sets it too).
os.environ.setdefault('SPARCLI_NO_TTY', '1')
