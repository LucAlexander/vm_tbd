#!/bin/bash
for rom_file in *.rom; do
	base_name=$(basename "$rom_file" .rom)
	echo -e "\e[1;32m$base_name\e[0m"
	./vm -r "$rom_file"
	echo ""
done
