#!/bin/bash

count=0;
total=0;

for i in $( awk '{ print $3; }' $1 | tr -d 'ops/s')
do
	total=$(echo $total+$i | bc )
	((count++))
done
echo "scale=2; $total / $count" | bc
