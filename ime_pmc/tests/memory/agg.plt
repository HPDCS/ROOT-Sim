set terminal png size 800,500
set output 'comp.png'

set style line 2 lc rgb 'black' lt 1 lw 1
set style data histogram
set style histogram cluster gap 1
set style fill pattern border -1
set boxwidth 0.9
set xtics format ""
set grid ytics

set title "Frequency comparison"
plot "agg.dat" using 2:xtic(1) title "1024" ls 2, \
            '' using 3 title "2048" ls 2, \
            '' using 4 title "4096" ls 2, \
            '' using 5 title "8192" ls 2, \
  ""  using 0:($2+.1):(sprintf("%3d",$2)) with labels notitle
