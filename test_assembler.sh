#!/bin/bash
for asm_file in examples/*.asm; do
	base_name=$(basename "$asm_file" .asm)
	rom_file="${base_name}.rom"
	echo -e "\e[1;32m$base_name\e[0m"
	./vm -a "$asm_file" -o "$rom_file"
done
