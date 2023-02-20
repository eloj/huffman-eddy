#!/bin/bash
INFILE=${1:-./huffman-eddy}
cp $INFILE testin
./huffman-eddy e testin testin.huff
if [ $? -eq 0 ]; then
	./huffman-eddy d testin testin.out && sha256sum $INFILE testin.out
else
	echo "Oops, I think the encoder may have crashed?"
fi
