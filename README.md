
# Toy Huffman Compressor & Decompressor (WIP)

WARNING: This repository is a work-in-progress, and will see frequent rebasing!

This code was written for educational purposes, and should under no circumstance
be deployed into the world. It is incomplete, with known defects and missing functionality.

By design it does not feature the sort of error- and bounds-checking
required in an adversarial environment.

[![Build status](https://github.com/eloj/huffman-eddy/workflows/build/badge.svg)](https://github.com/eloj/huffman-eddy/actions/workflows/c-cpp.yml)

# Status

WIP that works for the most part, EXCEPT:

* With no length-limiting some (large) inputs will raise an assert and crash the encoder.
* Only features full-size (one-level) decode table support (memory inefficient).
* The driver is a mess, and barebones.
* The bitio code is poor and possibly buggy.
* Shock full of debug code.

# TODO

* Implement Length-Limiting of the Huffman codes.
* Many more things, some of which are mentioned in the source code.

All code is provided under the [MIT License](LICENSE).

