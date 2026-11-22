PYTHON ?= python3

.DEFAULT_GOAL := verify
.PHONY: all setup build verify test clean

all: verify

setup:
	$(PYTHON) tools/bootstrap.py
	$(PYTHON) tools/build_historical.py r8604
	$(PYTHON) tools/compiler_arena.py
	$(PYTHON) tools/build_linux.py
	$(PYTHON) tools/build_wine.py

build:
	$(PYTHON) tools/build.py

verify: build
	$(PYTHON) tools/rom.py

test:
	$(PYTHON) -m unittest discover -s tools

clean:
	$(PYTHON) tools/build.py --clean
