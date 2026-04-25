set terminal pngcairo size 1400,600
set output 'tiempos_lineas.png'
set title "Tiempos de fases del GC"
set xlabel "Ciclo de GC"
set ylabel "Tiempo (ms)"
set grid
set key outside right top

plot 'tiempos.tsv' using 1:($2*1000) with lines title 'Marcar'     lc rgb '#4C72B0', \
     ''            using 1:($3*1000) with lines title 'Recolectar' lc rgb '#DD8452', \
     ''            using 1:($4*1000) with lines title 'Total'      lc rgb '#C44E52'