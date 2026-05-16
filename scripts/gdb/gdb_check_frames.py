import sys
import traceback

import gdb


def all_frames(grp, TGC_MARCO):
    p = grp['primero']
    while p:
        if p['tipo'] == TGC_MARCO:
            yield p
        p = p['siguiente']


def really_all_frames(ctx, TGC_MARCO):
    yield from all_frames(ctx['gc']['blanco_joven'], TGC_MARCO)
    yield from all_frames(ctx['gc']['blanco_en_la_pila'], TGC_MARCO)
    yield from all_frames(ctx['gc']['blanco_viejo'], TGC_MARCO)
    yield from all_frames(ctx['gc']['gris'], TGC_MARCO)
    yield from all_frames(ctx['gc']['negro'], TGC_MARCO)
    yield from all_frames(ctx['gc']['raices_viejas'], TGC_MARCO)
    yield from all_frames(ctx['gc']['recursos'], TGC_MARCO)


def total_frame_size(ctx, TGC_MARCO):
    return sum(p['num_bytes'] for p in really_all_frames(ctx, TGC_MARCO))

def frame_up(frm):
    frm = frm['k']['marco']
    while frm:
        yield frm
        frm = frm['k']['marco']

def stack_frames(frm):
    while frm:
        yield frm
        frm = frm['k']['marco']

def collect_stack_frames(frm, visited, bottom):
    xid = int(frm)
    if xid not in visited:
        visited[xid] = frm
        bottom.add(xid)
        for upfrm in frame_up(frm):
            up_xid = int(upfrm)
            bottom.discard(up_xid)
            if up_xid in visited:
                break
            visited[up_xid] = upfrm

def print_all_stack_trace(ctx, TGC_MARCO):
    visited = {}
    bottom = set()
    for p in really_all_frames(ctx, TGC_MARCO):
        collect_stack_frames(p.cast(gdb.lookup_type('pdcrt_marco').pointer()), visited, bottom)
    with open("kebab-traces.txt", "w") as f:
        for xid in bottom:
            frm = visited[xid]
            print('', file=f)
            print('=' * 20, hex(xid), '@', frm['gc']['grupo'], file=f)
            for frm in stack_frames(frm):
                print(' ', frm['debug_srcloc'], file=f)

class PdcrtFrameSizeCommand(gdb.Command):
    """Print a pdcrt_ctx's stack"""
    def __init__(self):
        super().__init__("pdcrt-framesize", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        ctx = gdb.parse_and_eval('ctx')
        TGC_MARCO = gdb.parse_and_eval('PDCRT_TGC_MARCO')
        print(total_frame_size(ctx, TGC_MARCO))

class PdcrtAllStackTracesCommand(gdb.Command):
    """Print a pdcrt_ctx's stack"""
    def __init__(self):
        super().__init__("pdcrt-all-stack-traces", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        ctx = gdb.parse_and_eval('ctx')
        TGC_MARCO = gdb.parse_and_eval('PDCRT_TGC_MARCO')
        try:
            print_all_stack_trace(ctx, TGC_MARCO)
        except Exception as exn:
            traceback.print_exception(exn)


PdcrtFrameSizeCommand()
PdcrtAllStackTracesCommand()