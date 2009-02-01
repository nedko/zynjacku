#!/usr/bin/env python
import os
import sys
import lv2

db = lv2.LV2DB()
plugins = db.getPluginList()

for uri in plugins:
    plugin = db.getPluginInfo(uri)
    if plugin == None:
        continue
    print "Plugin: %s" % plugin.name
    print "URI: %s" % uri
    if plugin.microname != None:
        print "Tiny name: %s" % plugin.microname
    if plugin.maintainers:
        print "Maintainers: %s" % plugin.maintainers
    print "License: %s" % plugin.license
    print "Classes: %s" % plugin.classes
    print "Required features: %s" % list(plugin.requiredFeatures)
    print "Optional features: %s" % list(plugin.optionalFeatures)
    print "Ports:"
    types = ["Audio", "Control", "Event", "Input", "Output", "LarslMidi"]
    for port in plugin.ports:
        extra = []
        for type in types:
            if port.__dict__["is" + type]:
                extra.append(type)
        for sp in ["defaultValue", "minimum", "maximum", "microname"]:
            if port.__dict__[sp] != None:
                extra.append("%s=%s" % (sp, repr(port.__dict__[sp])))
        print "%4s %-20s %-40s %s" % (port.index, port.symbol, port.name, ", ".join(extra))
        splist = port.scalePoints
        splist.sort(lambda x, y: cmp(x[1], y[1]))
        if len(splist):
            for sp in splist:
                print "       Scale point %s: %s" % (sp[1], sp[0])
        #print port
    print 
