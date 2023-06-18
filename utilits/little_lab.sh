#!/bin/bash

for ((d=2;d<=8;d++)); do
for ((i=0;i<=3;i++)); do
		./clear_cache.sh && ./dna_main $d $i && echo
done
done
