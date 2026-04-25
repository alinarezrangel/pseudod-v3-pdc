#!/usr/bin/env python3
"""
Parsea logs del recolector de basura y genera archivos TSV para Gnuplot.

Uso:
    python3 parse_gc_log.py <archivo_log> [directorio_salida]

Genera:
    - memoria.tsv:        Histórico de memoria total (heap) vs memoria usada al final.
    - tiempos.tsv:        Tiempos de marcar, recolectar y total por ciclo de GC.
    - memoria_programa.tsv: Memoria usada por el programa, intercalando antes/después de GC.
"""

import re
import sys
import os


# Multiplicadores para convertir tamaños a bytes.
SIZE_UNITS = {
    "b":   1,
    "Kib": 1024,
    "Mib": 1024 ** 2,
    "Gib": 1024 ** 3,
}

SIZE_RE = re.compile(r"([+-]?\d+(?:\.\d+)?)\s*([KMG]i)?b")
TIME_RE = re.compile(r"(\d+)\.(\d+)\s*\((\d+)\s+(\d+)\)")


def parse_size(text):
    """Convierte un tamaño tipo '45.174Kib' o '10.000Mib' a bytes (float)."""
    m = SIZE_RE.search(text)
    if not m:
        raise ValueError(f"No se pudo parsear el tamaño: {text!r}")
    value = float(m.group(1))
    unit = (m.group(2) or "") + "b"
    return value * SIZE_UNITS[unit]


def parse_time(text):
    """Convierte un tiempo tipo '0.000 (500 300)' a segundos (float).

    El formato es 'SECS.MSECS (USECS NSECS)' donde los cuatro campos son
    fraccionales y aditivos:
        tiempo = SECS + MSECS/1e3 + USECS/1e6 + NSECS/1e9
    Por ejemplo, '0.000 (500 300)' = 0.0005003 segundos.
    """
    m = TIME_RE.search(text)
    if not m:
        raise ValueError(f"No se pudo parsear el tiempo: {text!r}")
    secs  = int(m.group(1))
    msecs = int(m.group(2))
    usecs = int(m.group(3))
    nsecs = int(m.group(4))
    return secs + msecs * 1e-3 + usecs * 1e-6 + nsecs * 1e-9


def parse_log(lines):
    """Parsea las líneas del log y devuelve una lista de registros (dicts)."""
    records = []
    i = 0
    n = len(lines)
    while i < n:
        line = lines[i].strip()
        if "inicio GC:" in line:
            # Se espera un bloque de 7 líneas consecutivas.
            if i + 6 >= n:
                break
            try:
                inicio  = parse_size(lines[i].split(":", 1)[1])
                heap    = parse_size(lines[i + 1].split(":", 1)[1])
                marcar  = parse_time(lines[i + 2].split(":", 1)[1])
                recol   = parse_time(lines[i + 3].split(":", 1)[1])
                total   = parse_time(lines[i + 4].split(":", 1)[1])
                mem     = parse_size(lines[i + 5].split(":", 1)[1])
                delta   = parse_size(lines[i + 6].split(":", 1)[1])
            except (ValueError, IndexError) as e:
                sys.stderr.write(f"Advertencia: bloque malformado en línea {i + 1}: {e}\n")
                i += 1
                continue
            records.append({
                "inicio": inicio,
                "heap":   heap,
                "marcar": marcar,
                "recol":  recol,
                "total":  total,
                "mem":    mem,
                "delta":  delta,
            })
            i += 7
        else:
            i += 1
    return records


def write_memoria(records, path):
    """TSV con: ciclo, heap_total, mem_usada_final (en bytes)."""
    with open(path, "w") as f:
        f.write("# ciclo\theap_bytes\tmem_usada_bytes\n")
        for idx, r in enumerate(records):
            f.write(f"{idx}\t{r['heap']:.0f}\t{r['mem']:.0f}\n")


def write_tiempos(records, path):
    """TSV con: ciclo, marcar, recolectar, total (en segundos)."""
    with open(path, "w") as f:
        f.write("# ciclo\tmarcar_s\trecolectar_s\ttotal_s\n")
        for idx, r in enumerate(records):
            f.write(f"{idx}\t{r['marcar']:.9f}\t{r['recol']:.9f}\t{r['total']:.9f}\n")


def write_memoria_programa(records, path):
    """TSV con la memoria usada por el programa, intercalando los valores
    de 'inicio GC' (antes) y 'mem' (después de GC).

    Cada ciclo de GC produce dos puntos:
        - tiempo 2*i:     memoria justo antes de iniciar el GC.
        - tiempo 2*i + 1: memoria justo después del GC.
    """
    with open(path, "w") as f:
        f.write("# punto\tmem_bytes\tfase\n")
        punto = 0
        for r in records:
            f.write(f"{punto}\t{r['inicio']:.0f}\tantes\n")
            punto += 1
            f.write(f"{punto}\t{r['mem']:.0f}\tdespues\n")
            punto += 1


def main():
    if len(sys.argv) < 2:
        sys.stderr.write(__doc__)
        sys.exit(1)

    log_path = sys.argv[1]
    out_dir = sys.argv[2] if len(sys.argv) >= 3 else "."
    os.makedirs(out_dir, exist_ok=True)

    with open(log_path) as f:
        lines = f.readlines()

    records = parse_log(lines)
    sys.stderr.write(f"Se parsearon {len(records)} ciclos de GC.\n")

    write_memoria(records,           os.path.join(out_dir, "memoria.tsv"))
    write_tiempos(records,           os.path.join(out_dir, "tiempos.tsv"))
    write_memoria_programa(records,  os.path.join(out_dir, "memoria_programa.tsv"))

    sys.stderr.write(f"Archivos TSV generados en: {out_dir}\n")


if __name__ == "__main__":
    main()
