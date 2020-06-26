#!/bin/bash

# Get the average number in input file

count=0;
total=0;

for i in $(awk '{print $1}' $1)
do
	total=$(echo $total+$i | bc)
	((count++))
done
echo "scale=2; $total / $count" | bc
