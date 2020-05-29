#!/bin/bash

count=0;
totalops=0;
totalmbs=0;

for i in {1..100}
do
	j=$(filebench -f $1 | grep "Summary" | awk '{ print $6; print $10; }')
	mb=$(echo "$j" | sed -n 2p | tr -d 'mb/s')
	ops=$(echo "$j" | sed -n 1p)	
	totalops=$(echo "$totalops+$ops" | bc )
	totalmbs=$(echo "$totalmbs+$mb" | bc )
	((count++))
	echo "Total Ops: $totalops"
	echo "Total mbs: $totalmbs"
	echo "Count: $count"
done
echo -n "Average mbps: "
echo "scale=2; $totalmbs / $count" | bc

echo -n "average ops per sec: "
echo "scale=2; $totalops / $count" | bc

