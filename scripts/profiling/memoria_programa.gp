set terminal pngcairo size 1200,600
set output 'memoria_programa.png'
set title "Memoria del programa (intercalada antes/después de GC)"
set xlabel "Punto de medición"
set ylabel "Memoria (KiB)"
set grid
plot 'memoria_programa.tsv' using 1:($2/1024) with linespoints title 'Memoria del programa'