set terminal epscairo font 'Linux-Libertine, 14'

set xlabel "Frequency"

unset key

set auto x
set grid y
set yrange [0:*]

set style data histogram
set style fill solid border -1
set style histogram gap 1

set ylabel "ms"
set output "ime0.eps"
plot    'ime0.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime1.eps"
plot    'ime1.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime2.eps"
plot    'ime5.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime10.eps"
plot    'ime10.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime25.eps"
plot    'ime25.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime50.eps"
plot    'ime50.dat' u 2:xtic(1);

set ylabel "ms"
set output "ime100.eps"
plot    'ime100.dat' u 2:xtic(1);
