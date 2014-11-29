# Run traffic benchmark

# Autonomic
reset
set ta 1.4
set complete-calls 20000
set p 10
set np 24
set nprc 1024
set A
set output-dir auto
run pcs

# Incremental
reset
set ta 1.4
set complete-calls 20000
set p 10
set np 24
set nprc 1024
set A
set inc
set output-dir inc-forced
run pcs

# Full
reset
set ta 1.4
set complete-calls 20000
set p 10
set np 24
set nprc 1024
set A
set full
set output-dir full-forced
run pcs

# Incremental
reset
set ta 1.4
set complete-calls 20000
set p 10
set np 24
set nprc 1024
set inc
set output-dir inc
run pcs

# Full
reset
set ta 1.4
set complete-calls 20000
set p 10
set np 24
set nprc 1024
set full
set output-dir full
run pcs
