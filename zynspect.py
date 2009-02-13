#!/usr/bin/env python
import os
import sys
from distutils import sysconfig

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
    print "Binary: " + plugin.binary
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
        if len(port.properties):
            s = list()
            for prop in port.properties:
                if prop in lv2.port_property_names:
                    s.append(lv2.port_property_names[prop])
                else:
                    s.append(prop)
            extra.append("properties=%s" % ",".join(s))
        if len(port.contexts):
            s = list()
            for context in port.contexts:
                if context in lv2.context_names:
                    s.append(lv2.context_names[context])
                else:
                    s.append(context)
            extra.append("contexts=%s" % ",".join(s))
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
            print "        Type: " + ui.type
            print "        Binary: " + ui.binary
            print "        Required features: " + repr(ui.requiredFeatures)
            print "        Optional features: " + repr(ui.optionalFeatures)
        print
    print

    print "Sources:"
    for source in plugin.sources:
        print "    " + source
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
