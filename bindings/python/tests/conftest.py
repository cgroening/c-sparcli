"""Put the in-place-built package on the path so ``pytest`` needs no install."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "src"))
