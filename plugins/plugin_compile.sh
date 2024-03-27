#!/bin/bash

for x in *.c; do
	test "$x" == "plugins.c" && continue;
	file=$(basename "$x");
	file=${file%.*}
	echo "Compiling $file"
	gcc -c -fpic -o $file.o $file.c
	gcc -shared -o $file.so $file.o
done
