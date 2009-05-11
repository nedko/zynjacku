import re
import os
import sys
import glob
import zynjacku_ttl

lv2 = "http://lv2plug.in/ns/lv2core#"
lv2evt = "http://lv2plug.in/ns/ext/event#"
lv2str = "http://lv2plug.in/ns/dev/string-port#"
lv2ctx = "http://lv2plug.in/ns/dev/contexts#"
rdf = "http://www.w3.org/1999/02/22-rdf-syntax-ns#"
rdfs = "http://www.w3.org/2000/01/rdf-schema#"
epi = "http://lv2plug.in/ns/dev/extportinfo#"
rdf_type = rdf + "type"
rdfs_see_also = rdfs + "seeAlso"
rdfs_label = rdfs + "label"
rdfs_subclass_of = rdfs + "subClassOf"
tinyname_uri = "http://lv2plug.in/ns/dev/tiny-name"
foaf = "http://xmlns.com/foaf/0.1/"
doap = "http://usefulinc.com/ns/doap#"
lv2ui = "http://lv2plug.in/ns/extensions/ui#"
lv2ui_ui = lv2ui + "ui"
lv2ui_binary = lv2ui + "binary"
lv2preset = "http://lv2plug.in/ns/dev/presets#"
lv2preset_preset = lv2preset + "Preset"
lv2preset_appliesTo = lv2preset + "appliesTo"
lv2preset_hasPreset = lv2preset + "hasPreset"
lv2preset_value = lv2preset + "value"
dc = "http://dublincore.org/documents/dcmi-namespace/"
dc_title = dc + "title"

event_type_names = {
    "http://lv2plug.in/ns/ext/midi#MidiEvent" : "MIDI"
}

port_property_names = {
    "http://lv2plug.in/ns/lv2core#reportsLatency": "reportsLatency",
    "http://lv2plug.in/ns/lv2core#toggled": "toggled",
    "http://lv2plug.in/ns/lv2core#integer": "integer",
    "http://lv2plug.in/ns/lv2core#connectionOptional": "connectionOptional",
    "http://lv2plug.in/ns/lv2core#sampleRate": "sampleRate",
    "http://lv2plug.in/ns/dev/extportinfo#hasStrictBounds": "hasStrictBounds",
    "http://lv2plug.in/ns/dev/extportinfo#logarithmic": "logarithmic",
    "http://lv2plug.in/ns/dev/extportinfo#notAutomatic": "notAutomatic",
    "http://lv2plug.in/ns/dev/extportinfo#trigger": "trigger",
    "http://lv2plug.in/ns/dev/extportinfo#outputGain": "outputGain",
    "http://lv2plug.in/ns/dev/extportinfo#reportsBpm": "reportsBpm",
}

context_names = {
    "http://lv2plug.in/ns/dev/contexts#MessageContext": "MessageContext",
}

def uniq_seq(seq):
    return {}.fromkeys(seq).keys()

class DumpRDFModel:
    def addTriple(self, s, p, o):
        print "%s [%s] %s" % (s, p, repr(o))

class SimpleRDFModel:
    def __init__(self):
        self.bySubject = {}
        self.byPredicate = {}
        #self.byObject = {}
        self.byClass = {}
        self.object_sources = {}
    def getByType(self, classname):
        if classname in self.byClass:
            return self.byClass[classname]
        return []
    def getByPropType(self, propname):
        if propname in self.byPredicate:
            return self.byPredicate[propname]
        return []
    def getProperty(self, subject, props, optional = False, single = False):
        #print "getProperty(%s, %s)" % (repr(subject), repr(props))
        if type(props) is list:
            prop = props[0]
        else:
            prop = props
        if type(subject) is str or type(subject) is unicode:
            if not self.bySubject.has_key(subject):
                return None
            subject = self.bySubject[subject]
        elif type(subject) is dict:
            pass
        else:
            #print "subject type is %s" % type(subject)
            if single:
                return None
            else:
                return []
        anyprops = set()
        if prop in subject:
            for o in subject[prop]:
                anyprops.add(o)
        if type(props) is list:
            if len(props) > 1:
                result = set()
                for v in anyprops:
                    if single:
                        value = self.getProperty(v, props[1:], optional = optional, single = True)
                        if value != None:
                            return value
                    else:
                        result |= set(self.getProperty(v, props[1:], optional = optional, single = False))
                if single:
                    return None
                else:
                    return list(result)
        if single:
            if len(anyprops) > 0:
                if len(anyprops) > 1:
                    raise Exception, "More than one value of " + prop
                return list(anyprops)[0]
            else:
                return None
        return list(anyprops)
        
                
    def addTriple(self, s, p, o, source=None):
        #if p == lv2 + "binary":
        #    print 'binary "%s" of %s found' % (o, s)
        if not self.object_sources.has_key(o):
            self.object_sources[o] = set()
        self.object_sources[o].add(source)
        if p == rdf_type:
            p = "a"
        #if p == 'a' and o == lv2preset_preset:
        #    print 'preset "%s" found' % s

        if s not in self.bySubject:
            self.bySubject[s] = {}
        if p not in self.bySubject[s]:
            self.bySubject[s][p] = []
        self.bySubject[s][p].append(o)

        if p not in self.byPredicate:
            self.byPredicate[p] = {}
        if s not in self.byPredicate[p]:
            self.byPredicate[p][s] = []
        self.byPredicate[p][s].append(o)

        #if o not in self.byObject:
        #    self.byObject[o] = {}
        #if p not in self.byObject[o]:
        #    self.byObject[o][p] = []
        #self.byObject[o][p].append(s)

        if p == "a":
            if not self.object_sources.has_key(s):
                self.object_sources[s] = set()
            self.object_sources[s].add(source)
            if o in self.byClass:
                self.byClass[o].append(s)
            else:
                self.byClass[o] = [s]
    def copyFrom(self, src):
        for s in src.bySubject:
            po = src.bySubject[s]
            for p in po:
                for o in po[p]:
                    self.addTriple(s, p, o)
    def dump(self):
        for s in self.bySubject.keys():
            for p in self.bySubject[s].keys():
                print "%s %s %s" % (s, p, self.bySubject[s][p])

def parseTTL(uri, content, model, debug):
    # Missing stuff: translated literals, blank nodes
    if debug:
        print "Parsing: %s" % uri
    prefixes = {}
    spo_stack = []
    spo = ["", "", ""]
    item = 0
    anoncnt = 1
    for x in zynjacku_ttl.scan_string(content):
        if x[0] == '':
            continue
        if x[0] == "URI_": x = ('URI', x[1][1:-1])
        if x[0] == "float": x = ('number', float(x[1]))
        if x[0] == 'prefix':
            spo[0] = "@prefix"
            item = 1
            continue
        elif (x[0] == '.' and spo_stack == []) or x[0] == ';' or x[0] == ',':
            if item == 3:
                if spo[0] == "@prefix":
                    prefixes[spo[1][:-1]] = spo[2]
                else:
                    model.addTriple(spo[0], spo[1], spo[2], uri)
                if x[0] == '.': item = 0
                elif x[0] == ';': item = 1
                elif x[0] == ',': item = 2
            else:
                if x[0] == '.':
                    item = 0
                elif item != 0:
                    raise Exception, uri+": Unexpected " + x[0]
        elif x[0] == "prnot" and item < 3:
            prnot = x[1].split(":")
            if item != 0 and spo[0] == "@prefix":
                spo[item] = x[1]
            elif prnot[0] == "_":
                spo[item] = uri + "#" + prnot[1]
            else:
                if prnot[0] not in prefixes:
                    print 'WARNING %s: Prefix %s not defined. Ignoring %s:%s' % (uri, prnot[0], prnot[0], prnot[1])
                else:
                    spo[item] = prefixes[prnot[0]] + prnot[1]
            item += 1
        elif (x[0] == 'URI' or x[0] == "string" or x[0] == "number" or (x[0] == "symbol" and x[1] == "a" and item == 1)) and (item < 3):
            if x[0] == "URI" and x[1] == "":
                x = ("URI", uri)
            elif x[0] == "URI" and x[1].find(":") == -1 and x[1] != "" and x[1][0] != "/":
                # This is quite silly
                x = ("URI", os.path.dirname(uri) + "/" + x[1])
            spo[item] = x[1]
            item += 1
        elif x[0] == '[':
            if item != 2:
                raise Exception, "Incorrect use of ["
            uri2 = uri + "$anon$" + str(anoncnt)
            spo[2] = uri2
            spo_stack.append(spo)
            spo = [uri2, "", ""]
            item = 1
            anoncnt += 1
        elif x[0] == ']' or x[0] == ')':
            if item == 3:
                model.addTriple(spo[0], spo[1], spo[2], uri)
                item = 0
            spo = spo_stack[-1]
            spo_stack = spo_stack[:-1]
            item = 3
        elif x[0] == '(':
            if item != 2:
                raise Exception, "Incorrect use of ("
            uri2 = uri + "$anon$" + str(anoncnt)
            spo[2] = uri2
            spo_stack.append(spo)
            spo = [uri2, "", ""]
            item = 2
            anoncnt += 1
        else:
            print uri + ": Unexpected: " + repr(x)

class LV2Port(object):
    def __init__(self):
        pass
    def connectableTo(self, port):
        if not ((self.isInput and port.isOutput) or (self.isOutput and port.isInput)):
            return False
        if self.isAudio != port.isAudio or self.isControl != port.isControl or self.isEvent != port.isEvent:
            return False
        if not self.isAudio and not self.isControl and not self.isEvent:
            return False
        return True

class LV2Plugin(object):
    def __init__(self):
        pass
        
class LV2UI(object):
    def __init__(self):
        pass
        
class LV2Preset(object):
    def __init__(self):
        pass
        
class LV2DB:
    def __init__(self, sources=[], debug = False):
        self.debug = debug
        self.sources = sources
        self.initManifests()
        
    def initManifests(self):
        if os.environ.has_key("LV2_PATH"):
            lv2path = os.environ["LV2_PATH"].split(':')
        else:
            lv2path = []

            if os.environ.has_key("HOME"):
                if sys.platform == "darwin":
                    lv2path.append(os.environ["HOME"] + "/Library/Audio/Plug-Ins/LV2")
                else:
                    lv2path.append(os.environ["HOME"] + "/.lv2")

            if sys.platform == "darwin":
                lv2path.append("/Library/Audio/Plug-Ins/LV2")

            lv2path += ["/usr/local/lib/lv2", "/usr/lib/lv2"]

            print "LV2_PATH not set, defaulting to %s" % repr(lv2path)

        self.manifests = SimpleRDFModel()
        self.paths = {}
        self.plugin_info = dict()
        if not self.sources:
            # Scan manifests
            for dir in lv2path:
                for bundle in glob.iglob(dir + "/*.lv2"):
                    fn = bundle+"/manifest.ttl"
                    if os.path.exists(fn):
                        parseTTL(fn, file(fn).read(), self.manifests, self.debug)
            # Read all specifications from all manifests
            if lv2 + "Specification" in self.manifests.byClass:
                specs = self.manifests.getByType(lv2 + "Specification")
                filenames = set()
                for spec in specs:
                    subj = self.manifests.bySubject[spec]
                    if rdfs_see_also in subj:
                        for fn in subj[rdfs_see_also]:
                            filenames.add(fn)
                for fn in filenames:
                    parseTTL(fn, file(fn).read(), self.manifests, self.debug)
            self.plugins = self.manifests.getByType(lv2 + "Plugin")
        else:
            for source in self.sources:
                parseTTL(source, file(source).read(), self.manifests, self.debug)
            #fn = "/usr/lib/lv2/lv2core.lv2/lv2.ttl"
            #parseTTL(fn, file(fn).read(), self.manifests, self.debug)
            self.plugins = set(self.manifests.getByType(lv2 + "Plugin"))
        self.categories = set()
        self.category_paths = []
        self.add_category_recursive([], lv2 + "Plugin")
        
    def add_category_recursive(self, tree_pos, category):
        cat_name = self.manifests.getProperty(category, rdfs_label, single = True, optional = True)
        if not cat_name:
            return
        self.category_paths.append(((tree_pos + [cat_name])[1:], category))
        self.categories.add(category)
        items = self.manifests.byPredicate[rdfs_subclass_of]
        for subj in items:
            if subj in self.categories:
                continue
            for o in items[subj]:
                if o == category and subj not in self.categories:
                    self.add_category_recursive(list(tree_pos) + [cat_name], subj)
        
    def get_categories(self):
        return self.category_paths
        
    def getPluginList(self):
        return self.plugins
        
    def getPluginInfo(self, uri):
        #print "getting info for plugin " + uri
        if not self.manifests.bySubject.has_key(uri):
            #print 'no subject "%s"' % uri
            return None

        sources = []
        if uri not in self.plugin_info:
            if not self.sources:
                world = SimpleRDFModel()
                world.sources = set()
                world.copyFrom(self.manifests)
                seeAlso = self.manifests.bySubject[uri][rdfs_see_also]
                try:
                    for doc in seeAlso:
                        # print "Loading " + doc + " for plugin " + uri
                        parseTTL(doc, file(doc).read(), world, self.debug)
                        world.sources.add(doc)
                    self.plugin_info[uri] = world                
                except Exception, e:
                    print "ERROR %s: %s" % (uri, str(e))
                    return None
                for source in self.manifests.object_sources[uri]:
                    world.sources.add(source)
                sources = world.sources
            else:
                self.plugin_info[uri] = self.manifests

        info = self.plugin_info[uri]

        dest = LV2Plugin()
        dest.uri = uri

        dest.binary = info.getProperty(uri, lv2 + "binary", optional = True)
        if not dest.binary:
            #print "No binary"
            return None
        dest.binary = dest.binary[0]

        dest.name = info.getProperty(uri, doap + 'name', optional = True)
        if not dest.name:
            return None
        dest.name = dest.name[0]

        dest.license = info.bySubject[uri][doap + 'license'][0]
        dest.classes = info.bySubject[uri]["a"]
        dest.requiredFeatures = info.getProperty(uri, lv2 + "requiredFeature", optional = True)
        dest.optionalFeatures = info.getProperty(uri, lv2 + "optionalFeature", optional = True)
        dest.microname = info.getProperty(uri, tinyname_uri, optional = True)
        if len(dest.microname):
            dest.microname = dest.microname[0]
        else:
            dest.microname = None
        dest.maintainers = []
        if info.bySubject[uri].has_key(doap + "maintainer"):
            for maintainer in info.bySubject[uri][doap + "maintainer"]:
                maintainersubj = info.bySubject[maintainer]
                maintainerdict = {}
                maintainerdict['name'] = info.getProperty(maintainersubj, foaf + "name")[0]
                homepages = info.getProperty(maintainersubj, foaf + "homepage")
                if homepages:
                    maintainerdict['homepage'] = homepages[0]
                mboxes = info.getProperty(maintainersubj, foaf + "mbox")
                if mboxes:
                    maintainerdict['mbox'] = mboxes[0]
                dest.maintainers.append(maintainerdict)

        ports = []
        portDict = {}
        porttypes = {
            "isAudio" : lv2 + "AudioPort",
            "isControl" : lv2 + "ControlPort",
            "isEvent" : lv2evt + "EventPort",
            "isString" : lv2str + "StringPort",
            "isInput" : lv2 + "InputPort",
            "isOutput" : lv2 + "OutputPort",
            "isLarslMidi" : "http://ll-plugins.nongnu.org/lv2/ext/MidiPort",
        }
        
        for port in info.bySubject[uri][lv2 + "port"]:
            psubj = info.bySubject[port]
            pdata = LV2Port()
            pdata.uri = port
            pdata.index = int(info.getProperty(psubj, lv2 + "index")[0])
            pdata.symbol = info.getProperty(psubj, lv2 + "symbol")[0]
            pdata.name = info.getProperty(psubj, lv2 + "name")[0]
            classes = set(info.getProperty(psubj, "a"))
            pdata.classes = classes
            for pt in porttypes.keys():
                pdata.__dict__[pt] = porttypes[pt] in classes
            sp = info.getProperty(psubj, lv2 + "scalePoint")
            if sp and len(sp):
                splist = []
                for pt in sp:
                    name = info.getProperty(pt, rdfs_label, optional = True, single = True)
                    if name != None:
                        value = info.getProperty(pt, rdf + "value", optional = True, single = True)
                        if value != None:
                            splist.append((name, value))
                pdata.scalePoints = splist
            else:
                pdata.scalePoints = []
            if pdata.isControl:
                pdata.defaultValue = info.getProperty(psubj, [lv2 + "default"], optional = True, single = True)
            elif pdata.isString:
                pdata.defaultValue = info.getProperty(psubj, [lv2str + "default"], optional = True, single = True)
            else:
                pdata.defaultValue = None
            pdata.minimum = info.getProperty(psubj, [lv2 + "minimum"], optional = True, single = True)
            pdata.maximum = info.getProperty(psubj, [lv2 + "maximum"], optional = True, single = True)
            pdata.microname = info.getProperty(psubj, [tinyname_uri], optional = True, single = True)
            pdata.properties = set(info.getProperty(psubj, [lv2 + "portProperty"], optional = True))
            pdata.events = set(info.getProperty(psubj, [lv2evt + "supportsEvent"], optional = True))
            pdata.contexts = set(info.getProperty(psubj, [lv2ctx + "context"], optional = True))
            ports.append(pdata)
            portDict[pdata.uri] = pdata
        ports.sort(lambda x, y: cmp(x.index, y.index))
        dest.ports = ports
        dest.portDict = portDict

        if info.bySubject[uri].has_key(lv2ui_ui):
            dest.ui = uniq_seq(info.bySubject[uri][lv2ui_ui])
        else:
            dest.ui = []

        if info.bySubject[uri].has_key(lv2preset_hasPreset):
            dest.presets = info.bySubject[uri][lv2preset_hasPreset]
        else:
            dest.presets = []

        dest.sources = sources

        return dest

    def get_ui_info(self, plugin_uri, uri):
        info = self.plugin_info[plugin_uri]

        dest = LV2Plugin()
        dest.uri = uri
        dest.type = set(info.getProperty(uri, "a")).intersection(set([lv2ui + 'GtkUI', lv2ui + 'external'])).pop()
        dest.binary = info.getProperty(uri, lv2ui_binary)[0]
        dest.requiredFeatures = info.getProperty(uri, lv2ui + "requiredFeature", optional = True)
        dest.optionalFeatures = info.getProperty(uri, lv2ui + "optionalFeature", optional = True)

        return dest

    def get_preset_info(self, plugin_uri, uri):
        info = self.plugin_info[plugin_uri]

        dest = LV2Preset()
        dest.uri = uri
        dest.name = info.getProperty(uri, dc_title)[0]

        dest.ports = []
        for port in info.bySubject[uri][lv2 + "port"]:
            psubj = info.bySubject[port]
            pdata = LV2Port()
            pdata.uri = port
            pdata.symbol = info.getProperty(psubj, lv2 + "symbol")[0]
            pdata.value = info.getProperty(psubj, lv2preset_value)[0]
            dest.ports.append(pdata)

        return dest
