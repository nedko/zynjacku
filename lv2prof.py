#!/usr/bin/env python
import os
import sys
import tempfile
import cProfile
import pstats
import time

_proc_status = '/proc/%d/status' % os.getpid()

_scale = {'kB': 1024.0, 'mB': 1024.0*1024.0,
          'KB': 1024.0, 'MB': 1024.0*1024.0}

def _VmB(VmKey):
    '''Private.
    '''
    global _proc_status, _scale
     # get pseudo file  /proc/<pid>/status
    try:
        t = open(_proc_status)
        v = t.read()
        t.close()
    except:
        return 0.0  # non-Linux?
     # get VmKey line e.g. 'VmRSS:  9999  kB\n ...'
    i = v.index(VmKey)
    v = v[i:].split(None, 3)  # whitespace
    if len(v) < 3:
        return 0.0  # invalid format?
     # convert Vm value to bytes
    return float(v[1]) * _scale[v[2]]


def memory():
    '''Return memory usage in bytes.
    '''
    return _VmB('VmSize:')


def resident():
    '''Return resident memory usage in bytes.
    '''
    return _VmB('VmRSS:')


def stacksize():
    '''Return stack size in bytes.
    '''
    return _VmB('VmStk:')

mem0 = memory()

old_path = sys.path

inplace_libs = os.path.join(os.path.dirname(sys.argv[0]), ".libs")
if os.access(inplace_libs, os.R_OK):
    sys.path.append(inplace_libs)

try:
    from zynworld import lv2
except Exception, e:
    print "Failed to import zynjacku internal python modules"
    print repr(e)
    print "These directories were searched"
    for path in sys.path:
        print "    " + path
    sys.exit(1)

sys.path = old_path

def lv2scan():
    mem1 = memory()
    db = lv2.LV2DB()
    uris = db.getPluginList()
    total = len(uris)
    count = 0
    best = 0.0
    worst = 0.0
    sum = 0.0
    mem = 0.0
    oldmem = 0.0
    mem2 = memory()
    for uri in uris:
        percent = float(count) / total * 100
        count += 1
        msg = "[% 3u%%] [% 3u/% 3u] %s " % (percent, count, total, uri)
        print msg,
        sys.stdout.flush()
        t1 = time.time()
        info = db.getPluginInfo(uri)
        t2 = time.time()
        mem = memory()
        dt = t2 - t1
        print("%.3fs; %u triples; %.3f MiB" % (dt, info.triples, (mem - oldmem) / 1024 / 1024))
        oldmem = mem
        sum = sum + dt
        if count == 1:
            best = dt
            worst = dt
        else:
            if dt < best:
                best = dt
                print("NEW BEST: %.3fs" % best)
            elif dt > worst:
                worst = dt
                print("NEW WORST: %.3fs" % worst)
    print("Count: %u" % count)
    print("Best: %.3fs" % best)
    print("Worst: %.3fs" % worst)
    avg = sum / count
    print("Average: %.3fs" % avg)
    print("Memory on startup:   %.0f MiB" % (mem0 / 1024 / 1024))
    print("Memory after import: %.0f MiB" % (mem1 / 1024 / 1024))
    print("Memory after init:   %.0f MiB" % (mem2 / 1024 / 1024))
    print("Memory after scan:   %.0f MiB" % (mem / 1024 / 1024))

fh, path = tempfile.mkstemp()

cProfile.run('lv2scan()', path)
p = pstats.Stats(path)
p.sort_stats('time').print_stats(10)

os.close(fh)
