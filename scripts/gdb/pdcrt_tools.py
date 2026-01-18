import gdb
import traceback
from typing import Dict, Set
import re


class PdcrtPrinter:
    def __init__(self):
        self.visited_addresses: Set[int] = set()

    def reset_visited(self):
        self.visited_addresses.clear()

    def addr_to_int(self, addr) -> int:
        return int(str(addr), 16)

    def format_recv(self, obj) -> str:
        try:
            recv = obj['recv']
            human = str(recv)
            return human.split()[1][1:-1]
        except:
            return "???"

    def mark_visited(self, addr: int) -> bool:
        """Returns True if already visited"""
        if addr in self.visited_addresses:
            return True
        self.visited_addresses.add(addr)
        return False

    def print_obj(self, obj, indent="") -> None:
        """Pretty prints a pdcrt_obj"""
        if obj is None:
            print(f"{indent}<null>")
            return

        addr = self.addr_to_int(obj.address)
        recv = self.format_recv(obj)
        print(f"{indent}pdcrt_obj @ {hex(addr)} (recv={recv})")

        # Handle the union based on recv function
        if recv == "pdcrt_recv_entero":
            print(f"{indent}  ival = {obj['ival']}")
        elif recv == "pdcrt_recv_float":
            print(f"{indent}  fval = {obj['fval']}")
        elif recv == "pdcrt_recv_booleano":
            print(f"{indent}  bval = {obj['bval']}")
        elif recv == "pdcrt_recv_texto":
            texto = obj['texto']
            if texto:
                self.print_texto(texto.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_arreglo":
            arreglo = obj['arreglo']
            if arreglo:
                self.print_arreglo(arreglo.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_closure":
            closure = obj['closure']
            if closure:
                self.print_closure(closure.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_caja":
            caja = obj['caja']
            if caja:
                self.print_caja(caja.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_tabla":
            tabla = obj['tabla']
            if tabla:
                self.print_tabla(tabla.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_corrutina":
            coro = obj['coro']
            if coro:
                self.print_corrutina(coro.dereference(), indent + "  ")
        elif recv == "pdcrt_recv_instancia":
            inst = obj['inst']
            if inst:
                self.print_instancia(inst.dereference(), indent + "  ")

    def print_texto(self, texto, indent="") -> None:
        addr = self.addr_to_int(texto.address)
        self.mark_visited(addr)

        longitud = int(texto['longitud'])
        contenido = texto['contenido']
        vals = []
        for i in range(longitud):
            b = int(contenido[i])
            if b < 0:
                b += 256
            vals.append(b)
        bytes_val = bytes(vals)

        print(f"{indent}texto @ {hex(addr)} (len={longitud})")
        print(f"{indent}  content = {bytes_val}")

    def print_arreglo(self, arreglo, indent="") -> None:
        addr = self.addr_to_int(arreglo.address)
        if self.mark_visited(addr):
            print(f"{indent}<arreglo @ {hex(addr)} (already printed)>")
            return

        longitud = int(arreglo['longitud'])
        capacidad = int(arreglo['capacidad'])
        valores = arreglo['valores']

        print(f"{indent}arreglo @ {hex(addr)} (len={longitud}, cap={capacidad})")
        if valores:
            for i in range(longitud):
                print(f"{indent}  [{i}]:")
                self.print_obj(valores[i], indent + "    ")

    def print_closure(self, closure, indent="") -> None:
        addr = self.addr_to_int(closure.address)
        if self.mark_visited(addr):
            print(f"{indent}<closure @ {hex(addr)} (already printed)>")
            return

        num_capturas = int(closure['num_capturas'])
        f = closure['f']
        capturas = closure['capturas']

        print(f"{indent}closure @ {hex(addr)} (captures={num_capturas})")
        print(f"{indent}  f = {f}")
        for i in range(num_capturas):
            print(f"{indent}  capture[{i}]:")
            self.print_obj(capturas[i], indent + "    ")

    def print_caja(self, caja, indent="") -> None:
        addr = self.addr_to_int(caja.address)
        if self.mark_visited(addr):
            print(f"{indent}<caja @ {hex(addr)} (already printed)>")
            return

        print(f"{indent}caja @ {hex(addr)}")
        print(f"{indent}  valor:")
        self.print_obj(caja['valor'], indent + "    ")

    def print_tabla(self, tabla, indent="") -> None:
        addr = self.addr_to_int(tabla.address)
        if self.mark_visited(addr):
            print(f"{indent}<tabla @ {hex(addr)} (already printed)>")
            return

        mask = int(tabla['mascara'])
        ncols = int(tabla['num_colisiones'])
        ccols = int(tabla['cap_colisiones'])
        buckets = tabla['buckets']
        cols = tabla['colisiones']
        nbuckets = mask + 1
        nentries = int(tabla['buckets_ocupados'])

        print(f"{indent}tabla @ {hex(addr)} (buckets={nbuckets}, entries={nentries}, ncols={ncols})")

        printed_cols = set()

        if buckets:
            for i in range(nbuckets):
                bucket = buckets[i]
                if bucket['activo']:
                    print(f"{indent}  bucket[{i}]:")
                    current = bucket
                    while current:
                        if not current['activo']:
                            print(f"{indent}    ERROR: inactive")

                        print(f"{indent}    key:")
                        self.print_obj(current['llave'], indent + "      ")
                        print(f"{indent}    value:")
                        self.print_obj(current['valor'], indent + "      ")

                        if current['tiene_colision']:
                            idc = current['idc_colision']
                            if idc >= ccols:
                                print(f"{indent}    ERROR: invalid colision idc={idc}")
                            else:
                                print(f"{indent}  bucket -> collision[{idc}]:")
                                current = cols[idc]
                                printed_cols |= {int(idc)}
                        else:
                            current = None

        if cols:
            for i in range(ncols):
                bucket = cols[i]
                if bucket['activo'] and i not in printed_cols:
                    print(f"{indent}  collision[{i}]:")
                    print(f"{indent}    key:")
                    self.print_obj(bucket['llave'], indent + "      ")
                    print(f"{indent}    value:")
                    self.print_obj(bucket['valor'], indent + "      ")
                    if bucket['tiene_colision']:
                        print(f"{indent}    next: {bucket['idc_colision']}")

    def print_corrutina(self, coro, indent="") -> None:
        addr = self.addr_to_int(coro.address)
        if self.mark_visited(addr):
            print(f"{indent}<corrutina @ {hex(addr)} (already printed)>")
            return

        estado = int(coro['estado'])
        estados = ['INICIAL', 'SUSPENDIDA', 'EJECUTANDOSE', 'FINALIZADA']
        estado_str = estados[estado] if 0 <= estado < len(estados) else 'UNKNOWN'

        print(f"{indent}corrutina @ {hex(addr)} (estado={estado_str})")
        if estado == 0:  # PDCRT_CORO_INICIAL
            print(f"{indent}  punto_de_inicio:")
            self.print_obj(coro['punto_de_inicio'], indent + "    ")

    def print_instancia(self, inst, indent="") -> None:
        addr = self.addr_to_int(inst.address)
        if self.mark_visited(addr):
            print(f"{indent}<instancia @ {hex(addr)} (already printed)>")
            return

        num_atributos = int(inst['num_atributos'])
        atributos = inst['atributos']

        print(f"{indent}instancia @ {hex(addr)} (attrs={num_atributos})")
        print(f"{indent}  metodos:")
        self.print_obj(inst['metodos'], indent + "    ")
        print(f"{indent}  metodo_no_encontrado:")
        self.print_obj(inst['metodo_no_encontrado'], indent + "    ")

        for i in range(num_atributos):
            print(f"{indent}  attr[{i}]:")
            self.print_obj(atributos[i], indent + "    ")


class PdcrtPilaCommand(gdb.Command):
    """Print a pdcrt_ctx's stack"""
    def __init__(self):
        super().__init__("pdcrt-pila", gdb.COMMAND_DATA)
        self.printer = PdcrtPrinter()

    def invoke(self, arg, from_tty):
        try:
            # Evaluate the expression to get a pdcrt_ctx*
            ctx = gdb.parse_and_eval(arg)
            if ctx.type.code != gdb.TYPE_CODE_PTR:
                print(f"Error: Expected pointer type, got {ctx.type}")
                return

            tam_pila = int(ctx['tam_pila'])
            cap_pila = int(ctx['cap_pila'])
            pila = ctx['pila']

            print(f"Stack @ {ctx} (len={tam_pila}, cap={cap_pila})")
            self.printer.reset_visited()

            if pila:
                for i in range(tam_pila):
                    print(f"[{i}]:")
                    self.printer.print_obj(pila[i], "  ")
        except Exception as e:
            print(f"Error: {str(e)}")
            traceback.print_exc()


class PdcrtPCommand(gdb.Command):
    """Print a pdcrt_obj"""
    def __init__(self):
        super().__init__("pdcrt-p", gdb.COMMAND_DATA)
        self.printer = PdcrtPrinter()

    def invoke(self, arg, from_tty):
        try:
            obj = gdb.parse_and_eval(arg)
            self.printer.reset_visited()
            self.printer.print_obj(obj)
        except Exception as e:
            print(f"Error: {str(e)}")
            traceback.print_exc()


class PdcrtPPCommand(gdb.Command):
    """Print a pdcrt_cabecera_gc object"""
    def __init__(self):
        super().__init__("pdcrt-pp", gdb.COMMAND_DATA)
        self.printer = PdcrtPrinter()

    def invoke(self, arg, from_tty):
        try:
            header = gdb.parse_and_eval(arg)
            if header.type.code != gdb.TYPE_CODE_PTR:
                print(f"Error: Expected pointer type, got {header.type}")
                return

            # Get the tipo field to determine what kind of object this is
            tipo = int(header['tipo'])
            # Cast to the appropriate type based on tipo
            if tipo == 0:  # PDCRT_TGC_MARCO
                print("Marco objects not implemented")
            elif tipo == 1:  # PDCRT_TGC_TEXTO
                self.printer.reset_visited()
                self.printer.print_texto(header.cast(gdb.lookup_type('pdcrt_texto').pointer()).dereference())
            elif tipo == 2:  # PDCRT_TGC_ARREGLO
                self.printer.reset_visited()
                self.printer.print_arreglo(header.cast(gdb.lookup_type('pdcrt_arreglo').pointer()).dereference())
            elif tipo == 3:  # PDCRT_TGC_CLOSURE
                self.printer.reset_visited()
                self.printer.print_closure(header.cast(gdb.lookup_type('pdcrt_closure').pointer()).dereference())
            elif tipo == 4:  # PDCRT_TGC_CAJA
                self.printer.reset_visited()
                self.printer.print_caja(header.cast(gdb.lookup_type('pdcrt_caja').pointer()).dereference())
            elif tipo == 5:  # PDCRT_TGC_TABLA
                self.printer.reset_visited()
                self.printer.print_tabla(header.cast(gdb.lookup_type('pdcrt_tabla').pointer()).dereference())
            elif tipo == 6:  # PDCRT_TGC_VALOP
                print("Valop objects not implemented")
            elif tipo == 7:  # PDCRT_TGC_CORO
                self.printer.reset_visited()
                self.printer.print_corrutina(header.cast(gdb.lookup_type('pdcrt_corrutina').pointer()).dereference())
            elif tipo == 8:  # PDCRT_TGC_INSTANCIA
                self.printer.reset_visited()
                self.printer.print_instancia(header.cast(gdb.lookup_type('pdcrt_instancia').pointer()).dereference())
            else:
                print(f"Unknown object type: {tipo}")
        except Exception as e:
            print(f"Error: {str(e)}")
            traceback.print_exc()


class PdcrtLocateCommand(gdb.Command):
    """Print all paths to a given object."""
    def __init__(self):
        super().__init__("pdcrt-locate", gdb.COMMAND_DATA)
        self.printer = PdcrtPrinter()
        self.queue = []
        self.visited = set()
        self.parents = {}  # Maps address -> list of (parent_addr, description)
        self.target_addr = None

    def parse_cli(self, cli):
        x = [s.strip() for s in cli.split(",")]
        return x[0], x[1], ",".join(x[2:])

    def invoke(self, arg, from_tty):
        try:
            ctx_cli, m_cli, obj_cli = self.parse_cli(arg)
            ctx = gdb.parse_and_eval(ctx_cli)
            m = gdb.parse_and_eval(m_cli)
            obj = gdb.parse_and_eval(obj_cli)

            self.queue = []
            self.visited = set()
            self.parents = {}

            # Determine target address
            obj_type = obj.type
            if obj_type.code == gdb.TYPE_CODE_PTR:
                self.target_addr = self.printer.addr_to_int(obj)
            else:
                self.target_addr = self.printer.addr_to_int(obj.address)

            print(f"Searching for paths to object @ {hex(self.target_addr)}...")

            # Add all roots
            for i in range(int(ctx["tam_pila"])):
                self.add_root(ctx["pila"][i], f"ctx->pila[{i}]")

            if m:
                self.add_root(m, "m (current marco)")

            self.add_root(ctx["funcion_igualdad"], "ctx->funcion_igualdad")
            self.add_root(ctx["funcion_hash"], "ctx->funcion_hash")
            self.add_root(ctx["registro_de_espacios_de_nombres"], "ctx->registro_de_espacios_de_nombres")
            self.add_root(ctx["registro_de_modulos"], "ctx->registro_de_modulos")
            self.add_root(ctx["espacio_de_nombres_runtime"], "ctx->espacio_de_nombres_runtime")
            self.add_root(ctx["nombre_del_programa"], "ctx->nombre_del_programa")
            self.add_root(ctx["argv"], "ctx->argv")
            self.add_root(ctx["clase_objeto"], "ctx->clase_objeto")
            self.add_root(ctx["clase_numero"], "ctx->clase_numero")
            self.add_root(ctx["clase_arreglo"], "ctx->clase_arreglo")
            self.add_root(ctx["clase_boole"], "ctx->clase_boole")
            self.add_root(ctx["clase_procedimiento"], "ctx->clase_procedimiento")
            self.add_root(ctx["clase_texto"], "ctx->clase_texto")
            self.add_root(ctx["clase_tipo_nulo"], "ctx->clase_tipo_nulo")

            if ctx["continuacion_actual"]["marco"]:
                self.add_root(ctx["continuacion_actual"]["marco"], "ctx->continuacion_actual.marco")

            for field in ctx["textos_globales"].type.fields():
                self.add_root(ctx["textos_globales"][field.name], f"ctx->textos_globales.{field.name}")

            start = ctx["gc"]["raices_viejas"]["primero"]
            idx = 0
            while start:
                self.add_root(start, f"ctx->gc.raices_viejas[{idx}]")
                start = start["siguiente"]
                idx += 1

            # BFS exploration
            found = self.explore()

            if found:
                print(f"\n=== Object @ {hex(self.target_addr)} is REACHABLE ===")
                self.print_all_paths()
            else:
                print(f"\n=== Object @ {hex(self.target_addr)} is NOT REACHABLE from roots ===")

        except Exception as e:
            print(f"Error: {str(e)}")
            traceback.print_exc()

    def add_root(self, obj, description):
        """Add a root object to the queue"""
        if not obj:
            return
        self.queue.append((obj, description, None))

    def add_child(self, obj, description, parent_addr):
        """Add a child object during exploration"""
        if not obj:
            return
        self.queue.append((obj, description, parent_addr))

    def explore(self):
        """BFS to find all paths to target object"""
        found = False

        while self.queue:
            obj, desc, parent_addr = self.queue.pop(0)

            # Get address of this object
            obj_type = obj.type
            if obj_type.code == gdb.TYPE_CODE_PTR:
                if not obj:
                    continue
                addr = self.printer.addr_to_int(obj)
            else:
                addr = self.printer.addr_to_int(obj.address)

            # Track parent relationship
            if parent_addr is not None:
                if addr not in self.parents:
                    self.parents[addr] = []
                self.parents[addr].append((parent_addr, desc))
            else:
                # This is a root
                if addr not in self.parents:
                    self.parents[addr] = []
                self.parents[addr].append((None, desc))

            # Check if we found the target
            if addr == self.target_addr:
                found = True
                # Continue exploring to find all paths

            # Skip if already visited
            if addr in self.visited:
                continue
            self.visited.add(addr)

            # Explore children based on type
            tname = obj_type.name if obj_type.code != gdb.TYPE_CODE_PTR else obj_type.target().name

            if tname == "pdcrt_obj":
                self.explore_obj(obj, addr)
            elif tname == "pdcrt_marco":
                self.explore_marco(obj, addr)
            elif tname == "pdcrt_arreglo":
                self.explore_arreglo(obj, addr)
            elif tname == "pdcrt_closure":
                self.explore_closure(obj, addr)
            elif tname == "pdcrt_caja":
                self.explore_caja(obj, addr)
            elif tname == "pdcrt_tabla":
                self.explore_tabla(obj, addr)
            elif tname == "pdcrt_corrutina":
                self.explore_corrutina(obj, addr)
            elif tname == "pdcrt_instancia":
                self.explore_instancia(obj, addr)
            elif tname == "pdcrt_cabecera_gc":
                self.explore_cabecera_gc(obj, addr)

        return found

    RECVS_TO_FIELDS = {
        "texto": "texto",
        "marco": "marco",
        "arreglo": "arreglo",
        "caja": "caja",
        "tabla": "tabla",
        "corrutina": "coro",
        "instancia": "inst",
        "reubicado": "reubicado",
        "closure": "closure",
    }

    def explore_obj(self, obj, parent_addr):
        """Explore pdcrt_obj and its gc pointer"""
        recv = self.printer.format_recv(obj)
        if recv in ["pdcrt_recv_texto", "pdcrt_recv_arreglo", "pdcrt_recv_closure",
                    "pdcrt_recv_caja", "pdcrt_recv_tabla", "pdcrt_recv_corrutina",
                    "pdcrt_recv_instancia", "pdcrt_recv_reubicado"]:
            # These have gc-managed pointers
            field_name = recv.replace("pdcrt_recv_", "")
            ptr = obj[self.RECVS_TO_FIELDS[field_name]]
            if ptr:
                self.add_child(ptr, f".{field_name}", parent_addr)

    def explore_marco(self, marco, parent_addr):
        """Explore a marco's references"""
        if marco["k"]["marco"]:
            self.add_child(marco["k"]["marco"], ".k.marco", parent_addr)
        num_vars = int(marco["num_locales"]) + int(marco["num_capturas"])
        for i in range(num_vars):
            self.add_child(marco["locales_y_capturas"][i], f".locales_y_capturas[{i}]", parent_addr)

    def explore_arreglo(self, arreglo, parent_addr):
        """Explore an arreglo's elements"""
        longitud = int(arreglo["longitud"])
        for i in range(longitud):
            self.add_child(arreglo["valores"][i], f".valores[{i}]", parent_addr)

    def explore_closure(self, closure, parent_addr):
        """Explore a closure's captures"""
        num_capturas = int(closure["num_capturas"])
        for i in range(num_capturas):
            self.add_child(closure["capturas"][i], f".capturas[{i}]", parent_addr)

    def explore_caja(self, caja, parent_addr):
        """Explore a caja's value"""
        self.add_child(caja["valor"], ".valor", parent_addr)

    def explore_tabla(self, tabla, parent_addr):
        """Explore a tabla's buckets"""
        num_buckets = int(tabla["num_buckets"])
        for i in range(num_buckets):
            bucket = tabla["buckets"][i]
            if bucket["activo"]:
                j = 0
                current = bucket.address
                while current:
                    self.add_child(current["llave"], f".buckets[{i}][{j}].llave", parent_addr)
                    self.add_child(current["valor"], f".buckets[{i}][{j}].valor", parent_addr)
                    current = current["siguiente_colision"]
                    j += 1

    def explore_corrutina(self, coro, parent_addr):
        """Explore a corrutina's state"""
        estado = int(coro["estado"])
        if estado == 0:  # PDCRT_CORO_INICIAL
            self.add_child(coro["punto_de_inicio"], ".punto_de_inicio", parent_addr)
        elif estado == 1:  # PDCRT_CORO_SUSPENDIDA
            if coro["punto_de_suspencion"]["marco"]:
                self.add_child(coro["punto_de_suspencion"]["marco"], ".punto_de_suspencion.marco", parent_addr)
        elif estado == 2:  # PDCRT_CORO_EJECUTANDOSE
            if coro["punto_de_continuacion"]["marco"]:
                self.add_child(coro["punto_de_continuacion"]["marco"], ".punto_de_continuacion.marco", parent_addr)

    def explore_instancia(self, inst, parent_addr):
        """Explore an instancia's fields"""
        self.add_child(inst["metodos"], ".metodos", parent_addr)
        self.add_child(inst["metodo_no_encontrado"], ".metodo_no_encontrado", parent_addr)
        num_atributos = int(inst["num_atributos"])
        for i in range(num_atributos):
            self.add_child(inst["atributos"][i], f".atributos[{i}]", parent_addr)

    def explore_cabecera_gc(self, header, parent_addr):
        """Explore a cabecera_gc by casting to appropriate type"""
        tipo = int(header["tipo"])
        if tipo == 0:  # PDCRT_TGC_MARCO
            marco = header.cast(gdb.lookup_type('pdcrt_marco').pointer())
            self.explore_marco(marco, parent_addr)
        elif tipo == 2:  # PDCRT_TGC_ARREGLO
            arreglo = header.cast(gdb.lookup_type('pdcrt_arreglo').pointer())
            self.explore_arreglo(arreglo, parent_addr)
        elif tipo == 3:  # PDCRT_TGC_CLOSURE
            closure = header.cast(gdb.lookup_type('pdcrt_closure').pointer())
            self.explore_closure(closure, parent_addr)
        elif tipo == 4:  # PDCRT_TGC_CAJA
            caja = header.cast(gdb.lookup_type('pdcrt_caja').pointer())
            self.explore_caja(caja, parent_addr)
        elif tipo == 5:  # PDCRT_TGC_TABLA
            tabla = header.cast(gdb.lookup_type('pdcrt_tabla').pointer())
            self.explore_tabla(tabla, parent_addr)
        elif tipo == 7:  # PDCRT_TGC_CORO
            coro = header.cast(gdb.lookup_type('pdcrt_corrutina').pointer())
            self.explore_corrutina(coro, parent_addr)
        elif tipo == 8:  # PDCRT_TGC_INSTANCIA
            inst = header.cast(gdb.lookup_type('pdcrt_instancia').pointer())
            self.explore_instancia(inst, parent_addr)

    def print_all_paths(self):
        """Print all paths from roots to target"""
        paths = []
        self.find_paths_recursive(self.target_addr, [], paths)

        print(f"\nFound {len(paths)} path(s) to target:\n")
        for i, path in enumerate(paths, 1):
            print(f"Path {i}:")
            for step in path:
                print(f"  {step}")
            print()

    def find_paths_recursive(self, addr, current_path, all_paths):
        """Recursively find all paths from roots to given address"""
        if addr not in self.parents:
            return

        for parent_addr, desc in self.parents[addr]:
            if parent_addr is None:
                # Reached a root
                all_paths.append([desc] + current_path)
            else:
                # Recurse to parent
                new_path = [f"{hex(parent_addr)}{desc}"] + current_path
                self.find_paths_recursive(parent_addr, new_path, all_paths)


# Register commands
PdcrtPilaCommand()
PdcrtPCommand()
PdcrtPPCommand()
PdcrtLocateCommand()
