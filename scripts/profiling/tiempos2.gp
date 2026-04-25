set terminal pngcairo size 1200,600
set output 'tiempos.png'
set title "Tiempos de fases del GC"
set xlabel "Ciclo de GC"
set ylabel "Tiempo (ms)"
set grid ytics
set key outside right top

# Histograma apilado de las fases (marcar + recolectar) y línea para 'total'.
set style fill solid 0.8 border -1
set boxwidth 0.9
set style data histograms
set style histogram rowstacked

# Para superponer una línea sobre el histograma necesitamos un eje x numérico:
# usamos 'using ... :xtic(...)' implícito por el índice de fila.
plot 'tiempos.tsv' using ($2*1000)        title 'Marcar'      lc rgb '#4C72B0', \
     ''            using ($3*1000)        title 'Recolectar'  lc rgb '#DD8452', \
     ''            using 0:($4*1000)      title 'Total (medido)' \
                   with lines lw 2 lc rgb '#C44E52'