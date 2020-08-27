#!/bin/bash

echo "gvt, lp, cap, mode, pstate, threads, thr1, thr2, thr3, pow1, pow2, pow3"

# Scriptazzo2
for gvt in 500; do
for lp in 60 256 1024; do
for cap in 80; do
for mode in 8; do
for pstate in 1 2 3 4 5 6 7 8 9 10 11 12 13; do
for threads in $(seq 1 40); do

thr1=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-1/execution_stats | grep "EXPLOITING THROUGHPUT" | sed 's/.*: //' | sed 's/ events\/sec//')
thr2=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-2/execution_stats | grep "EXPLOITING THROUGHPUT" | sed 's/.*: //' | sed 's/ events\/sec//')
thr3=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-3/execution_stats | grep "EXPLOITING THROUGHPUT" | sed 's/.*: //' | sed 's/ events\/sec//')
pow1=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-1/execution_stats | grep "EXPLOITING ENERGY" | sed 's/.*: //' | sed 's/ W//')
pow2=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-2/execution_stats | grep "EXPLOITING ENERGY" | sed 's/.*: //' | sed 's/ W//')
pow3=$(cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-3/execution_stats | grep "EXPLOITING ENERGY" | sed 's/.*: //' | sed 's/ W//')

echo "$gvt,$lp,$cap,$mode,$pstate,$threads,$thr1,$thr2,$thr3,$pow1,$pow2,$pow3"

done
done
done
done
done
done

# Scriptazzo3
for repeat in 1 2 3; do
	for gvt in 200 500 1000 2000; do
		for lp in 60 256 1024; do
			for cap in 55 65; do
				for mode in 10 11; do

#cat $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$repeat/execution_stats
					echo ""

				done
			done
		done
	done
done


