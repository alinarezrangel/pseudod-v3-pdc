set terminal pngcairo size 1200,600
set output 'memoria.png'
set title "Uso de memoria"
set xlabel "Ciclo de GC"
set ylabel "Memoria (MiB)"
set grid
plot 'memoria.tsv' using 1:($2/1048576) with lines title 'Heap total', \
     ''            using 1:($3/1048576) with lines title 'Memoria usada'