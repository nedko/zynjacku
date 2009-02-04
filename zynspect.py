#!/usr/bin/env python
import os
import sys
import lv2

def show_plugin_info(plugin):
    print "Plugin: %s" % plugin.name
    print "URI: %s" % plugin.uri
    if plugin.microname != None:
        print "Tiny name: %s" % plugin.microname
    if plugin.maintainers:
        print "Maintainers: %s" % plugin.maintainers
    print "License: %s" % plugin.license
    print "Classes: %s" % plugin.classes
    print "Required features: %s" % list(plugin.requiredFeatures)
    print "Optional features: %s" % list(plugin.optionalFeatures)
    print "Ports:"
    types = ["Audio", "Control", "Event", "Input", "Output", "String", "LarslMidi"]
    for port in plugin.ports:
        extra = []
        for type in types:
            if port.__dict__["is" + type]:
                extra.append(type)
        for sp in ["defaultValue", "minimum", "maximum", "microname"]:
            if port.__dict__[sp] != None:
                extra.append("%s=%s" % (sp, repr(port.__dict__[sp])))
        if len(port.events):
            s = list()
            for evt in port.events:
                if evt in lv2.event_type_names:
                    s.append(lv2.event_type_names[evt])
                else:
                    s.append(evt)
            extra.append("events=%s" % ",".join(s))
        print "%4s %-20s %-40s %s" % (port.index, port.symbol, port.name, ", ".join(extra))
        splist = port.scalePoints
        splist.sort(lambda x, y: cmp(x[1], y[1]))
        if len(splist):
            for sp in splist:
                print "       Scale point %s: %s" % (sp[1], sp[0])
        #print port
    print

    if plugin.ui:
        print "UI bundles:"
        for ui_uri in plugin.ui:
            print "    " + ui_uri
            ui = db.get_ui_info(plugin.uri, ui_uri)
            print "        Binary: " + ui.binary
            print "        Required features: " + repr(ui.requiredFeatures)
            print "        Optional features: " + repr(ui.optionalFeatures)
        print
    print

def list_plugins(verbose):
    plugins = db.getPluginList()

    for uri in plugins:
        if verbose:
            plugin = db.getPluginInfo(uri)
            if plugin == None:
                continue
            show_plugin_info(plugin)
        else:
            print uri

db = lv2.LV2DB()

if len(sys.argv) >= 2:
    if sys.argv[1] == "dump":
        list_plugins(True)
    else:
        uri = sys.argv[1]

        plugin = db.getPluginInfo(uri)

        if plugin == None:
            print 'Plugin URI "%s" is unknown' % uri
            sys.exit(1)

        show_plugin_info(plugin)

else:
    list_plugins(False)
