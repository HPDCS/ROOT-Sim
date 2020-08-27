#!/bin/bash


for repeat in 1 2 3; do
for gvt in 200 500 1000 2000; do
for lp in 60 256 1024; do
for cap in 55 65; do
for mode in 10 11; do

cat << EOF > config.txt
STARTING_THREADS=10
STATIC_PSTATE=10
POWER_LIMIT=$cap.0
COMMITS_ROUND=8
ENERGY_PER_TX_LIMIT=50.000000
HEURISTIC_MODE=$mode
JUMP_PERCENTAGE=10.000000
DETECTION_MODE=2
DETECTION_TP_THRESHOLD=10.000000
DETECTION_PWR_THRESHOLD=10.000000
EXPLOIT_STEPS=250
EXTRA_RANGE_PERCENTAGE=10.000000
WINDOW_SIZE=10
HYSTERESIS=1.000000
POWER_UNCORE=0.5
CORE_PACKING=0
LOWER_SAMPLED_MODEL_PSTATE=2
EOF

wallclock=$(echo "2.75 * $gvt / 1" | bc)

echo "time timeout 25m ./phold --wt 40 --lp $lp --gvt $gvt --wallclock-time $wallclock --powercap 888 --silent-output --output-dir $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$repeat"

time timeout 25m ./phold --wt 40 --lp $lp --gvt $gvt --wallclock-time $wallclock --powercap 888 --silent-output --output-dir scriptazzo3-results-2/$gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$repeat

done
done
done
done
done
