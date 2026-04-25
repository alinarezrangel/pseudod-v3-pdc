set terminal pngcairo size 1200,600
set output 'tiempos.png'
set title "Tiempos de fases del GC"
set xlabel "Ciclo de GC"
set ylabel "Tiempo (ms)"
set style data histograms
set style histogram rowstacked
set style fill solid border -1
set boxwidth 0.9
plot 'tiempos.tsv' using ($2*1000) title 'Marcar', \
     ''            using ($3*1000) title 'Recolectar', \
     ''            using ($4*1000) title 'Total'