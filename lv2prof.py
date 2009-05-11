#!/usr/bin/env python
import os
import sys
import tempfile
import cProfile
import pstats

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
    for uri in uris:
        db.getPluginInfo(uri)

fh, path = tempfile.mkstemp()

cProfile.run('lv2scan()', path)
p = pstats.Stats(path)
p.sort_stats('time').print_stats(10)

os.close(fh)
