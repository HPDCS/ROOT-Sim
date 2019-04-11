set terminal epscairo font 'Linux-Libertine, 14'

set xlabel "Pages"

unset key
unset xtics

set auto x
set grid y
set yrange [0:*]

set style data histogram
set style fill solid border -1
set style histogram gap 1

set ylabel "Cumulative Accesses"
set output "oracle.eps"
plot    'oracle.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x0_0.eps"
plot    'ime0x0_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x1_0.eps"
plot    'ime0x1_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x4_0.eps"
plot    'ime0x4_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x10_0.eps"
plot    'ime0x10_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x100_0.eps"
plot    'ime0x100_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x1000_0.eps"
plot    'ime0x1000_0.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "ime0x10000_0.eps"
plot    'ime0x1000_0.dat' u 2:xtic(1);