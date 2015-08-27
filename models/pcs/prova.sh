~/rootsim/bin/rootsim-cc *.c
gdb --args ./a.out --np 1 --nprc 1 --gvt 1000 --gvt_snapshot_cycles 2 --scheduler stf --simulation_time 0 --lps_distribution block --cktrm_mode standard complete_calls 50000 
