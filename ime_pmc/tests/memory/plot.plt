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
set output "oracle.png"
plot    'oracle.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "hop1024.png"
plot    'hop1024.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "hop2048.png"
plot    'hop2048.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "hop4096.png"
plot    'hop4096.dat' u 2:xtic(1);

set ylabel "Cumulative Accesses"
set output "hop8192.png"
plot    'hop8192.dat' u 2:xtic(1);
