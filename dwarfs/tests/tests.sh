#!/bin/bash

count=0;
totalops=0;
totalmbs=0;

for i in {1..100}
do
	j=$(filebench -f $1 | grep "Summary" | awk '{ print $6; print $10; }')
	mb=$(echo "$j" | sed -n 2p | tr -d 'mb/s')
	ops=$(echo "$j" | sed -n 1p)	
	((count++))
	echo "$mb" >> $2
	echo "$ops" >> $3
	echo "Count: $i"

done
