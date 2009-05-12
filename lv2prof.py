#!/usr/bin/env python
import os
import sys
import tempfile
import cProfile
import pstats
import time

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
    db = lv2.LV2DB()
    uris = db.getPluginList()
    total = len(uris)
    count = 0
    best = 0.0
    worst = 0.0
    for uri in uris:
        percent = float(count) / total * 100
        count += 1
        msg = "[% 3u%%] [% 3u/% 3u] %s " % (percent, count, total, uri)
        print msg,
        sys.stdout.flush()
        t1 = time.time()
        db.getPluginInfo(uri)
        t2 = time.time()
        dt = t2 - t1
        print("%.3fs" % dt)
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

fh, path = tempfile.mkstemp()

cProfile.run('lv2scan()', path)
p = pstats.Stats(path)
p.sort_stats('time').print_stats(10)

os.close(fh)
