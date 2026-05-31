"""Shim so setuptools picks up the cffi extension.

All static metadata lives in ``pyproject.toml``; this file exists only to pass
the ``cffi_modules`` keyword (which has no pyproject equivalent) to setuptools.
The named ``ffibuilder`` in ``build_sparcli.py`` compiles the vendored sparcli C
sources into the ``sparcli._sparcli_cffi`` extension at build time.
"""

from setuptools import setup

setup(cffi_modules=["build_sparcli.py:ffibuilder"])
