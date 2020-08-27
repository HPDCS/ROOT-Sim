#!/bin/bash
for repeat in 4; do
for gvt in 500; do
for cap in 55; do
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

echo "time timeout 35m ./phold --wt 40 --lp 256 --gvt $gvt --simulation-time 22000  --powercap 888 --silent-output --output-dir phold-heuristics-results/$gvt-gvt-1024-lp-$cap-cap-$mode-mode-$repeat"
# time timeout 35m ./phold --wt 40 --lp 256 --gvt $gvt --simulation-time 24000  --powercap 888 --silent-output --output-dir heuristics-results/$gvt-gvt-1024-lp-$cap-cap-$mode-mode-$repeat

echo "time timeout 35m ./phold --wt 40 --lp 1024 --gvt $gvt --simulation-time 5000  --powercap 888 --silent-output --output-dir heuristics-results/$gvt-gvt-256-lp-$cap-cap-$mode-mode-$repeat"
time timeout 35m ./phold --wt 40 --lp 1024 --gvt $gvt --simulation-time 500  --powercap 888 --silent-output --output-dir heuristics-results/$gvt-gvt-256-lp-$cap-cap-$mode-mode-$repeat
done
done
done
done
