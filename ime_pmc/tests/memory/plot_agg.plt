set terminal epscairo font 'Linux-Libertine, 12'

set xlabel "Frequencies"

unset key

set auto x
set grid y
set yrange [0:*]

set style data histogram
set style fill solid 0.8
set style histogram gap 1

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg0.eps"
plot    'agg0.dat' u 2:xtic(1), "" using 0:($2+.05):(sprintf("%3.5f", $2)) with labels notitle, \
                                "" using 2 lt rgb "#ff0000" smooth cspline notitle with line;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg1.eps"
plot    'agg1.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg2.eps"
plot    'agg2.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg5.eps"
plot    'agg5.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg10.eps"
plot    'agg10.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg25.eps"
plot    'agg25.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg50.eps"
plot    'agg50.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;

set ylabel "Cumulative Accesses / Real Accesses "
set output "agg100.eps"
plot    'agg100.dat' u 2:xtic(1), "" using 0:($2+.1):(sprintf("%3.5f", $2)) with labels notitle;