#!/bin/bash
INFILE=${1:-./huffman-eddy}
cp $INFILE testin
./huffman-eddy e testin testin.huff
./huffman-eddy d testin testin.out && sha256sum $INFILE testin.out
