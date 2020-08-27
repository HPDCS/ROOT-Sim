#!/bin/bash


for repeat in 1 2 3 4 5 6 7 8 9; do
for gvt in 200 500 1000 2000; do
for lp in 1024; do
for cap in 80; do
for mode in 8; do
for pstate in 2; do
for threads in $(seq 1 4 40); do

cat << EOF > config.txt
STARTING_THREADS=$threads
STATIC_PSTATE=$pstate
POWER_LIMIT=$cap.0
COMMITS_ROUND=8
ENERGY_PER_TX_LIMIT=50.000000
HEURISTIC_MODE=$mode
JUMP_PERCENTAGE=10.000000
DETECTION_MODE=2
DETECTION_TP_THRESHOLD=10.000000
DETECTION_PWR_THRESHOLD=10.000000
EXPLOIT_STEPS=2500
EXTRA_RANGE_PERCENTAGE=10.000000
WINDOW_SIZE=10
HYSTERESIS=1.000000
POWER_UNCORE=0.5
CORE_PACKING=0
LOWER_SAMPLED_MODEL_PSTATE=2
EOF

echo "time ./phold --wt 40 --lp $lp --gvt $gvt --wallclock-time 1000000 --powercap 888  --output-dir $gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-$repeat"
time timeout 10m ./phold --wt 40 --lp $lp --gvt $gvt --wallclock-time 1000000 --powercap 888 >> results-throughput-variance-1024/$gvt-gvt-$lp-lp-$cap-cap-$mode-mode-$pstate-pstate-$threads-threads-$repeat

done
done
done
done
done
done
done
