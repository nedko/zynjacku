#!/usr/bin/env python
#

import sys
import re
import subprocess

#print "Looking in \"%s/lv2dynparam/lv2dynparam.h\" for #define-s matching \"%s\"" % (sys.argv[1], sys.argv[2])

header = open("%s/lv2dynparam/lv2dynparam.h" % sys.argv[1])

while True:
    line = header.readline()
    if not line:
        break

    m = re.compile("^#define\s+(\w+)\s+").match(line)
    if m:
        if re.compile(sys.argv[2]).match(m.group(1)):
            #print m.group(1)
            cfile = open("gen_py_constant.c", "w")
            cfile.write("#include <stdio.h>\n")
            cfile.write("#include <%s/lv2dynparam/lv2.h>\n" % sys.argv[1])
            cfile.write("#include <%s/lv2dynparam/lv2dynparam.h>\n" % sys.argv[1])
            cfile.write("int main()\n")
            cfile.write("{\n")
            cfile.write('  printf("PyModule_AddObject(m, \\"%s\\", PyString_FromString(\\"\" %s \"\\"));\\n");\n' % (m.group(1), m.group(1)))
            cfile.write("  return 0;\n")
            cfile.write("}\n")
            cfile.close()
            subprocess.call(["cc", "gen_py_constant.c", "-o", "gen_py_constant"])
            subprocess.call(["gen_py_constant"])
