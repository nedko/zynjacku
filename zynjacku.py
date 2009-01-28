#!/usr/bin/env python
#
# This file is part of zynjacku
#
# Copyright (C) 2006 Leonard Ritter <contact@leonard-ritter.com>
# Copyright (C) 2006,2007,2008,2009 Nedko Arnaudov <nedko@arnaudov.name>
#  
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

import os
import sys
import gtk
import gtk.glade
import gobject
import re
import time
import xml.dom.minidom
import cairo
from math import pi, sin, cos, atan2
from colorsys import hls_to_rgb, rgb_to_hls

old_path = sys.path
sys.path.insert(0, "%s/.libs" % os.path.dirname(sys.argv[0]))
import zynjacku_c as zynjacku
sys.path = old_path

try:
    import lash
except:
    print "Cannot load LASH python bindings, you want LASH unless you enjoy manual jack plumbing each time you use this app"
    lash = None

hint_uris = { "hidden": "http://home.gna.org/zynjacku/hints#hidden",
              "togglefloat": "http://home.gna.org/zynjacku/hints#togglefloat",
              "onesubgroup": "http://home.gna.org/zynjacku/hints#onesubgroup",
              }

def map_coords_linear(x,y):
    return x,1.0-y

def map_coords_spheric(x,y):
    nx = cos(x * 2 * pi) * y
    ny = -sin(x * 2 * pi) * y
    return nx, ny
    
def supershape(m, n1, n2, n3, phi):
    t1 = abs(cos(m * phi / 4.0))**n2
    t2 = abs(sin(m * phi / 4.0))**n3
    
    r = (t1+t2)**(1.0/n1)
    
    if r == 0:
        return 0,0
    r = 1.0/r
    return r

def draw_round_rectangle(ctx, x, y, w, h, arc1, arc2=None, arc3=None, arc4=None, ratio=0.618, tabwidth=0, tabheight=0):
    if arc2 == None:
        arc2 = arc1
    if arc3 == None:
        arc3 = arc1
    if arc4 == None:
        arc4 = arc1
    arch1 = arc1*ratio # arc handle length
    arch2 = arc2*ratio # arc handle length
    arch3 = arc3*ratio # arc handle length
    arch4 = arc4*ratio # arc handle length
    ctx.move_to(x+arc1, y)
    if tabwidth and tabheight:
        curvewidth = tabheight*2
        ctx.line_to(x+tabwidth, y)
        arch = curvewidth*0.5
        ctx.curve_to(x+tabwidth+arch, y, x+tabwidth+arch, y+tabheight, x+tabwidth+curvewidth, y+tabheight)
    ctx.line_to(x+w-arc2, y+tabheight)
    ctx.curve_to(x+w-arch2, y+tabheight, x+w, y+tabheight+arch2, x+w, y+tabheight+arc2)
    ctx.line_to(x+w, y+h-arc3)
    ctx.curve_to(x+w, y+h-arch3, x+w-arch3, y+h, x+w-arc3, y+h)
    ctx.line_to(x+arc4, y+h)
    ctx.curve_to(x+arch4, y+h, x, y+h-arch4, x, y+h-arc4)
    ctx.line_to(x, y+arc1)
    ctx.curve_to(x, y+arch1, x+arch1, y, x+arc1, y)
    ctx.close_path()

def get_peaks(f, tolerance=0.01, maxd=0.01, mapfunc=map_coords_linear):
    corners = 360
    yc = 1.0/corners
    peaks = []
    x0,y0 = 0.0,0.0
    t0 = -9999.0
    i0 = 0
    for i in xrange(int(corners)):
        p = i*yc
        a = f(p)
        x,y = mapfunc(p, a)
        if i == 0:
            x0,y0 = x,y
        t = atan2((y0 - y), (x0 - x)) / (2*pi)
        td = t - t0
        if (abs(td) >= tolerance):
            t0 = t
            peaks.append((x,y))
        x0,y0 = x,y
    return peaks

def make_knobshape(gaps, gapdepth):
    def knobshape_func(x):
        x = (x*gaps)%1.0
        w = 0.5
        g1 = 0.5 - w*0.5
        g2 = 0.5 + w*0.5
        if (x >= g1) and (x < 0.5):
            x = (x-g1)/(w*0.5)
            return 0.5 - gapdepth * x * 0.9
        elif (x >= 0.5) and (x < g2):
            x = (x-0.5)/(w*0.5)
            return 0.5 - gapdepth * (1-x) * 0.9
        else:
            return 0.5
    return get_peaks(knobshape_func, 0.03, 0.05, map_coords_spheric)
    
def hls_to_color(h,l,s):
    r,g,b = hls_to_rgb(h,l,s)
    return gtk.gdk.color_parse('#%04X%04X%04X' % (int(r*65535),int(g*65535),int(b*65535)))

def color_to_hls(color):
    string = color.to_string()
    r = int(string[1:5], 16) / 65535.0
    g = int(string[5:9], 16) / 65535.0
    b = int(string[9:13], 16) / 65535.0
    return rgb_to_hls(r, g, b)

MARKER_NONE = ''
MARKER_LINE = 'line'
MARKER_ARROW = 'arrow'
MARKER_DOT = 'dot'
    
LEGEND_NONE = ''
LEGEND_DOTS = 'dots' # painted dots
LEGEND_LINES = 'lines' # painted ray-like lines
LEGEND_RULER = 'ruler' # painted ray-like lines + a circular one
LEGEND_RULER_INWARDS = 'ruler-inwards' # same as ruler, but the circle is on the outside
LEGEND_LED_SCALE = 'led-scale' # an LCD scale
LEGEND_LED_DOTS = 'led-dots' # leds around the knob

class KnobTooltip:
    def __init__(self):
        self.tooltip_window = gtk.Window(gtk.WINDOW_POPUP)
        self.tooltip = gtk.Label()
        #self.tooltip.modify_fg(gtk.STATE_NORMAL, hls_to_color(0.0, 1.0, 0.0))
        self.tooltip_timeout = None
        vbox = gtk.VBox()
        vbox2 = gtk.VBox()
        vbox2.add(self.tooltip)
        vbox2.set_border_width(2)
        vbox.add(vbox2)
        self.tooltip_window.add(vbox)
        vbox.connect('expose-event', self.on_tooltip_expose)

    def show_tooltip(self, knob):
        text = knob.format_value(knob.value)
        self.tooltip.set_text(knob.format_value(knob.max_value))
        rc = knob.get_allocation()
        x,y = knob.window.get_origin()
        self.tooltip_window.show_all()
        w,h = self.tooltip_window.get_size()
        wx,wy = x+rc.x-w, y+rc.y+rc.height/2-h/2
        self.tooltip_window.move(wx,wy)
        rc = self.tooltip_window.get_allocation()
        self.tooltip_window.window.invalidate_rect((0,0,rc.width,rc.height), False)
        self.tooltip.set_text(text)
        if self.tooltip_timeout:
            gobject.source_remove(self.tooltip_timeout)
        self.tooltip_timeout = gobject.timeout_add(500, self.hide_tooltip)
            
    def hide_tooltip(self):
        self.tooltip_window.hide_all()
        
    def on_tooltip_expose(self, widget, event):
        ctx = widget.window.cairo_create()
        rc = widget.get_allocation()
        #ctx.set_source_rgb(*hls_to_rgb(0.0, 0.0, 0.5))
        #ctx.paint()
        ctx.set_source_rgb(*hls_to_rgb(0.0, 0.0, 0.5))
        ctx.translate(0.5, 0.5)
        ctx.set_line_width(1)
        ctx.rectangle(rc.x, rc.y,rc.width-1,rc.height-1)
        ctx.stroke()
        return False



knob_tooltip = None
def get_knob_tooltip():
    global knob_tooltip
    if not knob_tooltip:
        knob_tooltip = KnobTooltip()
    return knob_tooltip

class Knob(gtk.VBox):
    def __init__(self):
        gtk.VBox.__init__(self)
        self.gapdepth = 4
        self.gaps = 10
        self.value = 0.0
        self.min_value = 0.0
        self.max_value = 127.0
        self.fg_hls = 0.0, 0.7, 0.0
        self.legend_hls = None
        self.dragging = False
        self.start = 0.0
        self.digits = 2
        self.segments = 13
        self.label = ''
        self.marker = MARKER_LINE
        self.angle = (3.0/4.0) * 2 * pi
        self.knobshape = None
        self.legend = LEGEND_DOTS
        self.lsize = 2
        self.lscale = False
        self.set_double_buffered(True)
        self.connect('realize', self.on_realize)
        self.connect("size_allocate", self.on_size_allocate)
        self.connect('expose-event', self.on_expose)
        self.set_border_width(6)
        self.set_size_request(50, 50)
        self.tooltip_enabled = False
        self.adj = None

    def set_adjustment(self, adj):
        self.min_value = adj.lower
        self.max_value = adj.upper
        self.value = adj.value
        if self.adj:
            self.adj.disconnect(self.adj_id)
        self.adj = adj
        self.adj_id = adj.connect("value-changed", self.on_adj_value_changed)

    def is_sensitive(self):
        return self.get_property("sensitive")

    def format_value(self, value):
        return ("%%.%if" % self.digits) % value

    def show_tooltip(self):
        if self.tooltip_enabled:
            get_knob_tooltip().show_tooltip(self)
        
    def on_realize(self, widget):
        self.root = self.get_toplevel()
        self.root.add_events(gtk.gdk.ALL_EVENTS_MASK)
        self.root.connect('scroll-event', self.on_mousewheel)
        self.root.connect('button-press-event', self.on_left_down)
        self.root.connect('button-release-event', self.on_left_up)
        self.root.connect('motion-notify-event', self.on_motion)
        self.update_knobshape()
        
    def update_knobshape(self):
        rc = self.get_allocation()
        b = self.get_border_width()
        size = min(rc.width, rc.height) - 2*b
        gd = float(self.gapdepth*0.5) / size
        self.gd = gd
        self.knobshape = make_knobshape(self.gaps, gd)
        
    def set_legend_scale(self, scale):
        self.lscale = scale
        self.refresh()
        
    def set_legend_line_width(self, width):
        self.lsize = width
        self.refresh()
        
    def set_segments(self, segments):
        self.segments = segments
        self.refresh()
        
    def set_marker(self, marker):
        self.marker = marker
        self.refresh()
    
    def set_range(self, minvalue, maxvalue):
        self.min_value = minvalue
        self.max_value = maxvalue
        self.set_value(self.value)
        
    def quantize_value(self, value):
        scaler = 10**self.digits
        value = int((value*scaler)+0.5) / float(scaler)
        return value

    def on_adj_value_changed(self, adj):
        if self.value != adj.value:
            self.value = adj.value
            self.refresh()

    def set_value(self, value):
        oldval = self.value
        self.value = min(max(self.quantize_value(value), self.min_value), self.max_value)
        if self.value != oldval:
            if self.adj:
                self.adj.set_value(value)
            self.refresh()
        
    def get_value(self):
        return self.value
        
    def set_top_color(self, h, l, s):
        self.fg_hls = h,l,s
        self.refresh()
        
    def set_legend_color(self, h, l, s):
        self.legend_hls = h,l,s
        self.refresh()
        
    def get_top_color(self):
        return self.fg_hls
        
    def set_gaps(self, gaps):
        self.gaps = gaps
        self.knobshape = None
        self.refresh()
        
    def get_gaps(self):
        return self.gaps
        
    def set_gap_depth(self, gapdepth):
        self.gapdepth = gapdepth
        self.knobshape = None
        self.refresh()
        
    def get_gap_depth(self):
        return self.gapdepth
        
    def set_angle(self, angle):
        self.angle = angle
        self.refresh()
        
    def get_angle(self):
        return self.angle
        
    def set_legend(self, legend):
        self.legend = legend
        self.refresh()
        
    def get_legend(self):
        return self.legend
        
    def on_left_down(self, widget, event):
        #print "on_left_down"

        # dont drag insensitive widgets
        if not self.is_sensitive():
            return False

        if not sum(self.get_allocation().intersect((int(event.x), int(event.y), 1, 1))):
            return False
        if event.button == 1:
            #print "start draggin"
            self.startvalue = self.value
            self.start = event.y
            self.dragging = True
            self.show_tooltip()
            self.grab_add()
            return True
        return False

    def on_left_up(self, widget, event):
        #print "on_left_up"
        if not self.dragging:
            return False
        if event.button == 1:
            #print "stop draggin"
            self.dragging = False
            self.grab_remove()
            return True
        return False

    def on_motion(self, widget, event):
        #print "on_motion"

        # dont drag insensitive widgets
        if not self.is_sensitive():
            return False

        if self.dragging:
            x,y,state = self.window.get_pointer()
            rc = self.get_allocation()
            range = self.max_value - self.min_value
            scale = rc.height
            if event.state & gtk.gdk.SHIFT_MASK:
                scale = rc.height*8
            value = self.startvalue - ((y - self.start)*range)/scale
            oldval = self.value
            self.set_value(value)
            self.show_tooltip()
            if oldval != self.value:
                self.start = y
                self.startvalue = self.value
            return True
        return False
            
    def on_mousewheel(self, widget, event):

        # dont move insensitive widgets
        if not self.is_sensitive():
            return False

        if not sum(self.get_allocation().intersect((int(event.x), int(event.y), 1, 1))):
            return
        range = self.max_value - self.min_value
        minstep = 1.0 / (10**self.digits)
        if event.state & (gtk.gdk.SHIFT_MASK | gtk.gdk.BUTTON1_MASK):
            step = minstep
        else:
            step = max(self.quantize_value(range/25.0), minstep)
        value = self.value
        if event.direction == gtk.gdk.SCROLL_UP:
            value += step
        elif event.direction == gtk.gdk.SCROLL_DOWN:
            value -= step
        self.set_value(value)
        self.show_tooltip()

    def on_size_allocate(self, widget, allocation):
        #print allocation.x, allocation.y, allocation.width, allocation.height
        self.update_knobshape()

    def draw_points(self, ctx, peaks):
        ctx.move_to(*peaks[0])
        for peak in peaks[1:]:
            ctx.line_to(*peak)
        
    def draw(self, ctx):
        if not self.legend_hls:
            self.legend_hls = color_to_hls(self.style.fg[gtk.STATE_NORMAL])

        if not self.knobshape:
            self.update_knobshape()
        startangle = pi*1.5 - self.angle*0.5
        angle = ((self.value - self.min_value) / (self.max_value - self.min_value)) * self.angle + startangle
        rc = self.get_allocation()
        size = min(rc.width, rc.height)

        kh = self.get_border_width() # knob height

        ps = 1.0/size # pixel size
        ps2 = 1.0 / (size-(2*kh)-1) # pixel size inside knob
        ss = ps * kh # shadow size
        lsize = ps2 * self.lsize # legend line width
        # draw spherical
        ctx.translate(rc.x, rc.y)
        ctx.translate(0.5,0.5)
        ctx.translate(size*0.5, size*0.5)
        ctx.scale(size-(2*kh)-1, size-(2*kh)-1)
        if self.legend == LEGEND_DOTS:
            ctx.save()
            ctx.set_source_rgb(*hls_to_rgb(*self.legend_hls))
            dots = self.segments
            for i in xrange(dots):
                s = float(i)/(dots-1)
                a = startangle + self.angle*s
                ctx.save()
                ctx.rotate(a)
                r = lsize*0.5
                if self.lscale:
                    r = max(r*s,ps2)
                ctx.arc(0.5+lsize, 0.0, r, 0.0, 2*pi)
                ctx.fill()
                ctx.restore()
            ctx.restore()
        elif self.legend in (LEGEND_LINES, LEGEND_RULER, LEGEND_RULER_INWARDS):
            ctx.save()
            ctx.set_source_rgb(*hls_to_rgb(*self.legend_hls))
            dots = self.segments
            n = ps2*(kh-1)
            for i in xrange(dots):
                s = float(i)/(dots-1)
                a = startangle + self.angle*s
                ctx.save()
                ctx.rotate(a)
                r = n*0.9
                if self.lscale:
                    r = max(r*s,ps2)
                ctx.move_to(0.5+ps2+n*0.1, 0.0)
                ctx.line_to(0.5+ps2+n*0.1+r, 0.0)
                ctx.set_line_width(lsize)
                ctx.stroke()
                ctx.restore()
            ctx.restore()
            if self.legend == LEGEND_RULER:
                ctx.save()
                ctx.set_source_rgb(*hls_to_rgb(*self.legend_hls))
                ctx.set_line_width(lsize)
                ctx.arc(0.0, 0.0, 0.5+ps2+n*0.1, startangle, startangle+self.angle)
                ctx.stroke()
                ctx.restore()
            elif self.legend == LEGEND_RULER_INWARDS:
                ctx.save()
                ctx.set_source_rgb(*hls_to_rgb(*self.legend_hls))
                ctx.set_line_width(lsize)
                ctx.arc(0.0, 0.0, 0.5+ps2+n, startangle, startangle+self.angle)
                ctx.stroke()

        # draw shadow only for sensitive widgets that have height
        if self.is_sensitive() and kh:
            ctx.save()
            ctx.translate(ss, ss)
            ctx.rotate(angle)
            self.draw_points(ctx, self.knobshape)
            ctx.close_path()
            ctx.restore()
            ctx.set_source_rgba(0,0,0,0.3)
            ctx.fill()

        if self.legend in (LEGEND_LED_SCALE, LEGEND_LED_DOTS):
            ch,cl,cs = self.legend_hls
            n = ps2*(kh-1)
            ctx.save()
            ctx.set_line_cap(cairo.LINE_CAP_ROUND)
            ctx.set_source_rgb(*hls_to_rgb(ch,cl*0.2,cs))
            ctx.set_line_width(lsize)
            ctx.arc(0.0, 0.0, 0.5+ps2+n*0.5, startangle, startangle+self.angle)
            ctx.stroke()
            ctx.set_source_rgb(*hls_to_rgb(ch,cl,cs))
            if self.legend == LEGEND_LED_SCALE:
                ctx.set_line_width(lsize-ps2*2)
                ctx.arc(0.0, 0.0, 0.5+ps2+n*0.5, startangle, angle)
                ctx.stroke()
            elif self.legend == LEGEND_LED_DOTS:
                dots = self.segments
                dsize = lsize-ps2*2
                seg = self.angle/dots
                endangle = startangle + self.angle
                for i in xrange(dots):
                    s = float(i)/(dots-1)
                    a = startangle + self.angle*s
                    if ((a-seg*0.5) > angle) or (angle == startangle):
                        break
                    ctx.save()
                    ctx.rotate(a)
                    r = dsize*0.5
                    if self.lscale:
                        r = max(r*s,ps2)
                    ctx.arc(0.5+ps2+n*0.5, 0.0, r, 0.0, 2*pi)
                    ctx.fill()
                    ctx.restore()
            ctx.restore()
        pat = cairo.LinearGradient(-0.5, -0.5, 0.5, 0.5)
        pat.add_color_stop_rgb(1.0, 0.2,0.2,0.2)
        pat.add_color_stop_rgb(0.0, 0.3,0.3,0.3)
        ctx.set_source(pat)
        ctx.rotate(angle)
        self.draw_points(ctx, self.knobshape)
        ctx.close_path()
        ctx.fill_preserve()
        ctx.set_source_rgba(0.1,0.1,0.1,1)
        ctx.save()
        ctx.identity_matrix()
        ctx.set_line_width(1.0)
        ctx.stroke()
        ctx.restore()
        
        ctx.arc(0.0, 0.0, 0.5-self.gd, 0.0, pi*2.0)
        ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], max(self.fg_hls[1]*0.4,0.0), self.fg_hls[2]))
        ctx.fill()
        ctx.arc(0.0, 0.0, 0.5-self.gd-ps, 0.0, pi*2.0)
        ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], min(self.fg_hls[1]*1.2,1.0), self.fg_hls[2]))
        ctx.fill()
        ctx.arc(0.0, 0.0, 0.5-self.gd-(2*ps), 0.0, pi*2.0)
        ctx.set_source_rgb(*hls_to_rgb(*self.fg_hls))
        ctx.fill()

        # dont draw cap for insensitive widgets
        if not self.is_sensitive():
            return

        #~ ctx.set_line_cap(cairo.LINE_CAP_ROUND)
        #~ ctx.move_to(0.5-0.3-self.gd-ps, 0.0)
        #~ ctx.line_to(0.5-self.gd-ps*5, 0.0)
        
        if self.marker == MARKER_LINE:
            ctx.set_line_cap(cairo.LINE_CAP_BUTT)
            ctx.move_to(0.5-0.3-self.gd-ps, 0.0)
            ctx.line_to(0.5-self.gd-ps, 0.0)
            ctx.save()
            ctx.identity_matrix()
            ctx.translate(0.5,0.5)
            ctx.set_line_width(5)
            ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], min(self.fg_hls[1]*1.2,1.0), self.fg_hls[2]))
            ctx.stroke_preserve()
            ctx.set_line_width(3)
            ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], max(self.fg_hls[1]*0.4,0.0), self.fg_hls[2]))
            ctx.stroke()
            ctx.restore()
        elif self.marker == MARKER_DOT:
            ctx.arc(0.5-0.05-self.gd-ps*5, 0.0, 0.05, 0.0, 2*pi)
            ctx.save()
            ctx.identity_matrix()
            ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], min(self.fg_hls[1]*1.2,1.0), self.fg_hls[2]))
            ctx.stroke_preserve()
            ctx.set_line_width(1)
            ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], max(self.fg_hls[1]*0.4,0.0), self.fg_hls[2]))
            ctx.fill()
            ctx.restore()
        elif self.marker == MARKER_ARROW:
            ctx.set_line_cap(cairo.LINE_CAP_BUTT)
            ctx.move_to(0.5-0.3-self.gd-ps, 0.1)
            ctx.line_to(0.5-0.1-self.gd-ps, 0.0)
            ctx.line_to(0.5-0.3-self.gd-ps, -0.1)
            ctx.close_path()
            ctx.save()
            ctx.identity_matrix()
            #~ ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], min(self.fg_hls[1]*1.2,1.0), self.fg_hls[2]))
            #~ ctx.stroke_preserve()
            ctx.set_line_width(1)
            ctx.set_source_rgb(*hls_to_rgb(self.fg_hls[0], max(self.fg_hls[1]*0.4,0.0), self.fg_hls[2]))
            ctx.fill()
            ctx.restore()
        
    def refresh(self):
        rect = self.get_allocation()
        if self.window:
            self.window.invalidate_rect(rect, False)
        return True
        
    def on_expose(self, widget, event):
        self.context = self.window.cairo_create()
        self.draw(self.context)
        return False
        
class DecoBox(gtk.VBox):
    def __init__(self):
        gtk.VBox.__init__(self)
        self.arc1 = 0.0
        self.arc2 = None
        self.arc3 = None
        self.arc4 = None
        self.fg_hls = 0.0, 0.0, 0.6
        self.bg_hls = 0.6, 0.3, 0.2
        self.text_hls = 0.0, 1.0, 0.5
        self.filled = False
        self.tabwidth = 0
        self.tabheight = 6
        self.thickness = 1.0
        self.ratio = 0.382
        self.alpha = 1.0
        self.set_app_paintable(True)
        self.connect('expose-event', self.on_expose)
        self.vbox = gtk.VBox()
        hbox = gtk.HBox()
        self.pack_start(hbox, expand=False)
        self.pack_start(self.vbox)
        self.tabbox = hbox
        self.set_roundness(6, 6, 6, 6)
        
    def update_tabbox_size(self):
        self.tabbox.set_size_request(-1, self.tabheight+10)

    def set_label(self, text):
        self.label = text
        self.update_tabbox_size()
        self.refresh()

    def set_thickness(self, thickness):
        self.thickness = thickness
        self.refresh()
        
    def set_tab_size(self, width, height):
        self.tabwidth = width
        self.tabheight = height
        self.update_tabbox_size()
        self.refresh()
    
    def get_thickness(self):
        return self.thickness
        
    def set_roundness_ratio(self, ratio):
        self.ratio = ratio
        self.refresh()
        
    def get_roundness_ratio(self):
        return self.ratio
        
    def set_roundness(self, topleft, topright=None, bottomleft=None, bottomright=None):
        self.arc1 = topleft
        self.arc2 = topright
        self.arc3 = bottomleft
        self.arc4 = bottomright
        self.refresh()

    def set_alpha(self, alpha):
        self.alpha = alpha
        self.refresh()
        
    def get_alpha(self):
        return self.alpha

    def set_fg_color(self, h, l, s):
        self.fg_hls = h,l,s
        self.refresh()

    def get_fg_color():
        return self.fg_hsl

    def set_bg_color(self, h, l, s):
        self.bg_hls = h,l,s
        self.refresh()

    def get_bg_color():
        return self.bg_hsl

    def set_label_color(self, h, l, s):
        self.text_hls = h,l,s
        self.refresh()

    def get_label_color():
        return self.text_hsl
        
    def refresh(self):
        rc = self.get_allocation()
        if self.window:
            self.window.invalidate_rect(rc, False)
        return True
        
    def configure_font(self, ctx):
        ctx.select_font_face(
            "Bitstream Sans Vera", 
            cairo.FONT_SLANT_NORMAL,
            cairo.FONT_WEIGHT_BOLD)
        ctx.set_font_size(9)
        fo = cairo.FontOptions()
        fo.set_antialias(cairo.ANTIALIAS_GRAY)
        fo.set_hint_style(cairo.HINT_STYLE_NONE)
        fo.set_hint_metrics(cairo.HINT_METRICS_DEFAULT)
        ctx.set_font_options(fo)

    def draw(self, ctx):
        rc = self.get_allocation()
        x,y,w,h = rc.x, rc.y, rc.width, rc.height
        bw = self.get_border_width()
        x += bw
        y += bw
        w -= bw*2
        h -= bw*2
        
        label = self.label.upper()
        ctx.push_group()
        self.configure_font(ctx)
        xb, yb, fw, fh, xa, ya = ctx.text_extents(label)
        
        tw = max(self.tabwidth,xa+self.arc1+self.tabheight)

        ctx.set_source_rgb(*hls_to_rgb(*self.bg_hls))
        draw_round_rectangle(ctx, x, y, w, h, 
            arc1=self.arc1, arc2=self.arc2, arc3=self.arc3, arc4=self.arc4, 
            ratio=self.ratio, tabwidth=tw, tabheight=self.tabheight)

        ctx.fill()
        if self.thickness:
            ctx.set_source_rgb(*hls_to_rgb(*self.fg_hls))
            th = self.thickness*0.5
            draw_round_rectangle(ctx, x+th, y+th, w-self.thickness, h-self.thickness, 
                arc1=self.arc1, arc2=self.arc2, arc3=self.arc3, arc4=self.arc4, 
                ratio=self.ratio, tabwidth=tw, tabheight=self.tabheight)
            ctx.set_line_width(self.thickness)
            ctx.stroke()
            
        if label:
            ctx.set_source_rgb(*hls_to_rgb(*self.text_hls))
            ctx.translate(x+self.arc1*0.5,y+self.tabheight*0.5-yb)
            if self.thickness:
                ctx.translate(self.thickness+1, self.thickness+1)
            ctx.show_text(label)
            ctx.fill()
        ctx.pop_group_to_source()
        ctx.paint_with_alpha(self.alpha)

    def on_expose(self, widget, event):
        self.context = widget.window.cairo_create()
        self.draw(self.context)
        return False

class midi_led(gtk.EventBox):
    def __init__(self):
        gtk.EventBox.__init__(self)
        self.label = gtk.Label()
        #attrs = pango.AttrList()
        #font_attr =  pango.AttrFamily("monospace")
        #attrs.insert(font_attr)
        #self.label.set_attributes(attrs)
        self.add(self.label)

    def set(self, active):
        if active:
            self.modify_bg(gtk.STATE_NORMAL, gtk.gdk.Color(0, int(65535 * 0.5), 0))
        else:
            self.modify_bg(gtk.STATE_NORMAL, self.label.style.bg[gtk.STATE_NORMAL])

        self.label.set_text(" MIDI ")

class curve_widget(gtk.DrawingArea):
    def __init__(self, min_value, max_value, value):
        gtk.DrawingArea.__init__(self)

        self.connect("expose-event", self.on_expose)
        self.connect("size-request", self.on_size_request)
        self.connect("size_allocate", self.on_size_allocate)

        self.color_bg = gtk.gdk.Color(0,0,0)
        self.color_value = gtk.gdk.Color(int(65535 * 0.8), int(65535 * 0.7), 0)
        self.color_mark = gtk.gdk.Color(int(65535 * 0.2), int(65535 * 0.2), int(65535 * 0.2))
        self.width = 0
        self.height = 0
        self.margin = 5

        self.points = []
        self.full_range = False

        self.min_value = min_value
        self.max_value = max_value

        self.moving_point_cc = -1
        self.moving_point_value = value

    def add_point(self, cc, adj):
        i = 0
        for point in self.points:
            if point[0] > cc:
                break
            i += 1
        self.points.insert(i, [cc, adj])

    def set_full_range(self, full_range = True):
        self.full_range = full_range
        self.invalidate_all()

    def remove_point(self, cc):
        #print "removing point with cc value %u" % cc
        i = 0
        for point in self.points:
            #print "%u ?= %u" % (cc, point[0])
            if point[0] == cc:
                adj = point[1]
                #print "removed point %u -> %f (index %u)" % (cc, value, i)
                del(self.points[i])
                return adj
            i += 1

        print "point with cc value %u not found" % cc
        return None

    def change_point_cc(self, cc_value_old, cc_value_new):
        adj = self.remove_point(cc_value_old)
        self.add_point(cc_value_new, adj)
        self.invalidate_all()

    def set_moving_point_cc(self, cc_value, parameter_value):
        self.moving_point_cc = cc_value
        self.moving_point_value = parameter_value
        self.invalidate_all()

    def get_moving_point(self, min_value, max_value):
        if self.moving_point_cc >= 0:
            x = self.get_x(self.moving_point_cc)
            y = self.get_y(min_value, max_value, self.moving_point_value)

            return x, y

        y = self.get_y(min_value, max_value, self.moving_point_value)

        #print "searching for value %f" % self.moving_point_value
        prev_point = self.points[0]
        for point in self.points:
            #print "%f ? %f ? %f" % (prev_point[1].value, self.moving_point_value, point[1].value)
            if point[1].value == self.moving_point_value:
                x = self.get_x(float(point[0]))
                return x, y
            elif ((prev_point[1].value < self.moving_point_value and self.moving_point_value < point[1].value) or
                  (prev_point[1].value > self.moving_point_value and self.moving_point_value > point[1].value)):
                x1 = float(prev_point[0])
                y1 = prev_point[1].value
                x2 = float(point[0])
                y2 = point[1].value
                y3 = self.moving_point_value
                x3 = x1 + ((y3 - y1) * (x2 - x1) / (y2 - y1))
                #print "x = %f" % x3
                x = self.get_x(x3)
                return x, y
            prev_point = point
        return None

    def on_expose(self, widget, event):
        cairo_ctx = widget.window.cairo_create()

        # set a clip region for the expose event
        cairo_ctx.rectangle(event.area.x, event.area.y, event.area.width, event.area.height)
        cairo_ctx.clip()

        self.draw(cairo_ctx)

        return False

    def on_size_allocate(self, widget, allocation):
        #print allocation.x, allocation.y, allocation.width, allocation.height
        self.width = float(allocation.width)
        self.height = float(allocation.height)
        self.font_size = 10

    def on_size_request(self, widget, requisition):
        #print "size-request, %u x %u" % (requisition.width, requisition.height)
        requisition.width = 150
        requisition.height = 150
        return

    def invalidate_all(self):
        self.queue_draw_area(0, 0, int(self.width), int(self.height))

    def get_x(self, cc_value):
        x = self.margin + cc_value / 127.0 * (self.width - 2 * self.margin)
        #print "x(%f) -> %u" % (cc_value, x)
        return x

    def get_y(self, min_value, max_value, value):
        if max_value == min_value:
            y = self.height / 2.0
        else:
            v = 1.0 - (value - min_value) / (max_value - min_value)
            y = self.margin + v * (self.height - 2 * self.margin)

        #print "y(%f, [%f, %f]) -> %u" % (value, min_value, max_value, y)
        return y

    def draw(self, cairo_ctx):
        cairo_ctx.set_source_color(self.color_bg)
        cairo_ctx.rectangle(0, 0, self.width, self.height)
        cairo_ctx.fill()

        #cairo_ctx.set_source_color(self.color_mark)
        #cairo_ctx.rectangle(self.margin, self.margin, self.width - 2 * self.margin, self.height - 2 * self.margin)
        #cairo_ctx.stroke()

        if not self.points:
            return

        if self.full_range:
            min_value = self.min_value
            max_value = self.max_value
        else:
            max_value = min_value = self.points[0][1].value
            for point in self.points[1:]:
                if point[1].value > max_value:
                    max_value = point[1].value
                elif point[1].value < min_value:
                    min_value = point[1].value

        if min_value <= 0 and max_value >= 0:
            cairo_ctx.set_source_color(self.color_mark)
            cairo_ctx.set_line_width(1);

            y = self.get_y(min_value, max_value, 0)
            x = self.get_x(0)
            cairo_ctx.move_to(x, y)
            x = self.get_x(127)
            cairo_ctx.line_to(x, y)

            cairo_ctx.stroke()

        cairo_ctx.set_source_color(self.color_value)
        cairo_ctx.set_line_width(1);

        prev_point = False
        for point in self.points:
            x = self.get_x(point[0])
            y = self.get_y(min_value, max_value, point[1].value)
            #print x, y
            if not prev_point:
                cairo_ctx.move_to(x, y)
            else:
                cairo_ctx.line_to(x, y)
            prev_point = True

        cairo_ctx.stroke()

        pt = self.get_moving_point(min_value, max_value)
        if pt:
            cairo_ctx.arc(pt[0], pt[1], 3, 0, 2 * pi)
            cairo_ctx.fill()

class midiccmap:
    def __init__(self, map, parameter_name, min_value=0.0, max_value=1.0, value=0.5):
        self.parameter_name = parameter_name
        self.min_value = min_value
        self.max_value = max_value

        self.map = map
        self.map.connect("point-created", self.on_point_created)
        self.map.connect("point-removed", self.on_point_removed)
        self.map.connect("point-cc-changed", self.on_point_cc_changed)
        self.map.connect("point-value-changed", self.on_point_value_changed)
        self.map.connect("cc-no-assigned", self.on_cc_no_assigned)
        self.map.connect("cc-value-changed", self.on_cc_map_value_changed)

        self.window = gtk.Dialog(
            flags=gtk.DIALOG_MODAL | gtk.DIALOG_DESTROY_WITH_PARENT,
            buttons=(gtk.STOCK_UNDO, gtk.RESPONSE_NONE, gtk.STOCK_DELETE, gtk.RESPONSE_NO, gtk.STOCK_CANCEL, gtk.RESPONSE_REJECT, gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))

        # the grand vbox
        vbox = self.window.vbox
        #vbox.set_spacing(5)
        #self.window.add(vbox)

        # top hbox
        hbox_top = gtk.HBox()
        hbox_top.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(10, 0, 10, 10)
        align.add(hbox_top)
        vbox.pack_start(align, False)

        cc_no = map.get_cc_no()
        if cc_no == -1:
            self.adj_cc_no = gtk.Adjustment(-1, -1, 255, 1, 19)
        else:
            self.adj_cc_no = gtk.Adjustment(cc_no, 0, 255, 1, 19)
        self.adj_cc_no.connect("value-changed", self.on_cc_no_changed)
        cc_no = gtk.SpinButton(self.adj_cc_no, 0.0, 0)
        cc_no_box = gtk.HBox()
        cc_no_box.pack_start(gtk.Label("MIDI CC #"))
        cc_no_box.pack_start(cc_no)
        hbox_top.pack_start(cc_no_box, False)

        hbox_top.pack_start(gtk.Label("Show parameter range:"), False)
        button = gtk.RadioButton(None, "mapped")
        button.connect("toggled", self.on_set_full_range, False)
        hbox_top.pack_start(button, False)
        button = gtk.RadioButton(button, "full")
        button.connect("toggled", self.on_set_full_range, True)
        hbox_top.pack_start(button, False)

        # middle hbox
        hbox_middle = gtk.HBox()
        hbox_middle.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 0, 10, 10)
        align.add(hbox_middle)
        vbox.pack_start(align)

        vbox_left = gtk.VBox()
        vbox_left.set_spacing(5)
        hbox_middle.pack_start(vbox_left, False)

        self.ls = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gtk.Adjustment, bool, str)

        r = gtk.CellRendererText()
        c1 = gtk.TreeViewColumn("From", r, text=0)
        c2 = gtk.TreeViewColumn("->", r, text=4)
        c3 = gtk.TreeViewColumn("To", r, text=1)

        self.tv = gtk.TreeView(self.ls)
        self.tv.set_headers_visible(False)
        self.tv.append_column(c1)
        self.tv.append_column(c2)
        self.tv.append_column(c3)
        tv_box = gtk.VBox()

        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(3, 3, 10, 10)
        align.add(gtk.Label("Control points"))
        tv_box.pack_start(align, False)

        sw = gtk.ScrolledWindow()
        sw.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        sw.add(self.tv)
        tv_box.pack_start(sw)

        vbox_left.pack_start(tv_box, True, True)

        self.cc_value_delete_button = gtk.Button("Remove")
        self.cc_value_delete_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_delete_button, False)

        self.cc_value_new_button = gtk.Button("New")
        self.cc_value_new_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_new_button, False)

        self.cc_value_change_button = gtk.Button("Change CC value")
        self.cc_value_change_button.connect("clicked", self.on_button_clicked)
        vbox_left.pack_start(self.cc_value_change_button, False)

        self.curve = curve_widget(min_value, max_value, value)
        self.curve.set_size_request(250,250)
        frame = gtk.Frame()
        frame.set_shadow_type(gtk.SHADOW_ETCHED_OUT)
        frame.add(self.curve)
        hbox_middle.pack_start(frame, True, True)

        # bottom hbox
        hbox_bottom = gtk.HBox()
        hbox_bottom.set_spacing(10)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 2, 10, 10)
        align.add(hbox_bottom)
        vbox.pack_start(align, False)

        self.adj_cc_value = gtk.Adjustment(17, 0, 127, 1, 10)
        self.adj_cc_value.connect("value-changed", self.on_cc_value_changed)

        self.cc_value = gtk.SpinButton(None, 0.0, 0)
        self.cc_value.set_adjustment(self.adj_cc_value)
        label = gtk.Label("CC value")
        hbox_bottom.pack_start(label, False)
        hbox_bottom.pack_start(self.cc_value, False)

        hbox_bottom.pack_start(gtk.Label("-> %s" % self.parameter_name), False)

        self.value_knob = Knob()
        hbox_bottom.pack_start(self.value_knob, False)

        self.value_spin = gtk.SpinButton(digits=2)
        hbox_bottom.pack_start(self.value_spin, False)

        self.floating_adj = self.create_value_adjustment(value)
        self.smart_point_creation_enabled = True
        self.value_adj = None

        self.initial_points = True
        self.points = []
        self.map.get_points()
        self.initial_points = False

        self.tv.get_selection().connect("changed", self.on_selection_changed)

        self.tv.get_selection().select_path((0,))

        self.set_title()

    def create_value_adjustment(self, value):
        range = self.max_value - self.min_value
        adj = gtk.Adjustment(value, self.min_value, self.max_value, range / 100, range / 5)
        adj.connect("value-changed", self.on_value_change_request)
        return adj

    def on_point_created(self, map, cc_value, parameter_value):
        #print "on_point_created(%u, %f)" % (cc_value, parameter_value)

        prev_iter = None

        for row in self.ls:
            if int(row[0]) > cc_value:
                break
            prev_iter = row.iter
        
        adj = self.create_value_adjustment(parameter_value)
        self.curve.add_point(cc_value, adj)
        iter = self.ls.insert_after(prev_iter, [str(cc_value), "", adj, cc_value == 0 or cc_value == 127, "->"])
        self.update_point_value(iter, parameter_value)

        if self.initial_points:
            self.points.append([cc_value, parameter_value])

        self.tv.get_selection().select_iter(iter)
        self.set_value_adjustment(adj)
        self.curve.invalidate_all()

    def on_point_removed(self, map, cc_value):
        #print "on_point_removed(%u)" % cc_value

        selection = self.tv.get_selection()

        prev_iter = None
        row = None
        for row in self.ls:
            if int(row[0]) == cc_value:
                break
            prev_iter = row.iter

        if not row or row.iter == prev_iter:
            print "cannot find point to remove. cc value is %u" % cc_value
            return

        if row.iter != selection.get_selected()[1]:
            path = self.ls.get_path(self.current_row)
        else:
            path = None

        self.ls.remove(row.iter)

        if path:
            selection.select_path(path)

        self.curve.remove_point(cc_value)
        self.curve.invalidate_all()

    def on_point_cc_changed(self, map, cc_value_old, cc_value_new):
        #print "on_point_cc_changed(%u, %u)" % (cc_value_old, cc_value_new)
        self.curve.change_point_cc(cc_value_old, cc_value_new)
        for row in self.ls:
            if int(row[0]) == cc_value_old:
                row[0] = cc_value_new
                self.set_value_adjustment(row[2])
        self.curve.set_moving_point_cc(cc_value_new, self.map_cc_value(cc_value_new))

    def on_point_value_changed(self, map, cc_value, parameter_value):
        #print "on_point_value_changed(%u, %f)" % (cc_value, parameter_value)
        for row in self.ls:
            if int(row[0]) == cc_value:
                self.update_point_value(row.iter, parameter_value)
                self.curve.invalidate_all()

    def on_cc_no_assigned(self, map, cc_no):
        #print "on_cc_no_assigned(%u)" % cc_no
        self.adj_cc_no.value = cc_no

    def on_cc_map_value_changed(self, map, cc_value):
        #print "on_cc_map_value_changed(%u)" % cc_value
        self.adj_cc_value.value = cc_value

    def map_cc_value(self, cc_value):
        if len(self.ls) < 2:
            return None
        row = self.ls[0]
        x1 = float(row[0])
        y1 = row[2].value
        x3 = float(cc_value)
        for row in self.ls:
            x2 = float(row[0])
            y2 = row[2].value
            if int(x2) == cc_value:
                return y2
            if int(x2) > cc_value:
                return y1 + ((x3 - x1) * (y2 - y1) / (x2 - x1))
            x1 = x2
            y1 = y2
        return None

    def revert_points(self):
        path = self.ls.get_path(self.current_row)
        for row in self.ls:
            self.map.point_remove(int(row[0]))
        
        for point in self.points:
            self.map.point_create(point[0], point[1])
        if path[0] >= len(self.ls):
            path = (len(self.ls) - 1,)
        self.tv.get_selection().select_path(path)

    def run(self):
        self.window.show_all()
        while True:
            ret = self.window.run()
            if ret == gtk.RESPONSE_NONE: # revert/undo button pressed?
                #print "revert"
                self.revert_points()
                continue
            elif ret == gtk.RESPONSE_REJECT or ret == gtk.RESPONSE_DELETE_EVENT: # cancel button pressed? dialog window closed
                #print "cancel"
                self.revert_points()
                ret = False
            elif ret == gtk.RESPONSE_NO: # delete button pressed?
                #print "delete"
                ret = "delete"
            else:
                #print "ok"
                ret = True

            self.window.hide_all()
            return ret

    def set_title(self):
        title = 'Parameter "%s" MIDI CC' % self.parameter_name
        if self.adj_cc_no.value >= 0:
            title += " #%u" % int(self.adj_cc_no.value)
        title += ' map'
        self.window.set_title(title)

    def on_set_full_range(self, button, full_range):
        self.curve.set_full_range(full_range)

    def on_cc_no_changed(self, adj):
        self.set_title()
        if int(adj.value) != -1 and int(adj.lower) == -1:
            adj.lower = 0
        self.map.cc_no_assign(int(adj.value));

    def on_value_change_request(self, adj):
        #print "on_value_change_request() called. value = %f" % adj.value

        cc_value = int(self.adj_cc_value.value)

        for row in self.ls:
            if row[2] == adj:
                #print "found"
                self.map.point_parameter_value_change(int(row[0]), adj.value)
                self.curve.set_moving_point_cc(cc_value, self.map_cc_value(cc_value))
                return

        #print "not found"
        #self.curve.set_moving_point_cc(cc_value, adj.value)
        if self.smart_point_creation_enabled:
            self.map.point_create(cc_value, adj.value)

    def update_point_value(self, iter, value):
        self.ls[iter][1] = "%.2f" % value

    def on_selection_changed(self, obj):
        iter = self.tv.get_selection().get_selected()[1]
        self.current_row = iter
        if not iter:
            #print "selection gone"
            return
        row = self.ls[iter]
        #print "selected %s" % row[0]

        # is immutable?
        immutable = row[3]
        self.cc_value_delete_button.set_sensitive(not immutable)
        self.current_immutable = immutable

        self.set_value_adjustment(row[2])
        self.cc_value.set_value(int(row[0]))

        #self.on_cc_value_changed(row[2])

    def on_cc_value_changed(self, adj):
        #print "on_cc_value_changed"
        cc_value = int(adj.value)
        parameter_value = self.map_cc_value(cc_value)
        self.curve.set_moving_point_cc(cc_value, parameter_value)
        for row in self.ls:
            #print "%s ?= %s" % (row[0], int(adj.value))
            if int(row[0]) == int(adj.value):
                self.cc_value_new_button.set_sensitive(False)
                self.cc_value_change_button.set_sensitive(False)
                self.set_value_adjustment(row[2])
                return
        self.cc_value_new_button.set_sensitive(True)
        self.cc_value_change_button.set_sensitive(not self.current_immutable)

        self.set_value_adjustment(self.floating_adj)
        self.smart_point_creation_enabled = False
        self.floating_adj.value = parameter_value
        self.smart_point_creation_enabled = True

    def set_value_adjustment(self, adj):
        #print "set_value_adjustment"
        if self.value_adj == adj:
            return

        #if adj == self.floating_adj:
        #    print "floating adjustment"
        #else:
        #    print "point adjustment"

        self.value_adj = adj
        if self.value_knob:
            self.value_knob.set_adjustment(adj)
        self.value_spin.set_adjustment(adj)
        self.value_spin.set_value(adj.value)

    def on_button_clicked(self, button):
        if button == self.cc_value_change_button:
            #print "change cc value"
            self.map.point_cc_value_change(int(self.ls[self.current_row][0]), int(self.adj_cc_value.value))
        elif button == self.cc_value_new_button:
            #print "new cc value"
            self.map.point_create(int(self.adj_cc_value.value), self.value_adj.value)
        elif button == self.cc_value_delete_button:
            #print "delete cc value"
            self.map.point_remove(int(self.ls[self.current_row][0]))

# Synth window abstraction
class PluginUI(gobject.GObject):

    __gsignals__ = {
        'destroy':                      # signal name
        (
            gobject.SIGNAL_RUN_LAST,    # signal flags, when class closure is invoked
            gobject.TYPE_NONE,          # return type
            ()                          # parameter types
        )}

    def __init__(self, plugin):
        gobject.GObject.__init__(self)
        self.plugin = plugin

    def show(self):
        '''Show synth window'''

    def hide(self):
        '''Hide synth window'''

class PluginUICustom(PluginUI):
    def __init__(self, plugin):
        PluginUI.__init__(self, plugin)

        self.plugin.connect("custom-gui-off", self.on_window_destroy)

    def on_window_destroy(self, synth):
        #print "Custom GUI window destroy detected"
        self.emit('destroy')

    def show(self):
        '''Show synth window'''
        return self.plugin.ui_on()

    def hide(self):
        '''Hide synth window'''

        self.plugin.ui_off()

class PluginUIUniversalGroup(gobject.GObject):
    def __init__(self, window, parent_group, name, hints, context):
        gobject.GObject.__init__(self)
        self.window = window
        self.parent_group = parent_group
        self.group_name = name
        self.context = context
        self.hints = hints

    def child_param_add(self, obj):
        return

    def child_param_remove(self, obj):
        return

    def get_top_widget(self):
        return None

    def child_group_add(self, obj):
        return

    def child_group_remove(self, obj):
        return

    def on_child_group_appeared(self, group_name, hints, context):
        if hints.has_key(hint_uris['togglefloat']):
            group = PluginUIUniversalGroupToggleFloat(self.window, self, group_name, hints, context)
            self.window.defered.append(group)
            return group
        elif hints.has_key(hint_uris['onesubgroup']):
            group = PluginUIUniversalGroupOneSubGroup(self.window, self, group_name, hints, context)
            self.window.defered.append(group)
            return group
        else:
            return PluginUIUniversalGroupGeneric(self.window, self, group_name, hints, context)

class PluginUIUniversalParameter(gobject.GObject):
    def __init__(self, window, parent_group, name, context):
        gobject.GObject.__init__(self)
        self.window = window
        self.parent_group = parent_group
        self.parameter_name = name
        self.context = context

    def get_top_widget(self):
        return None

    def remove(self):
        self.parent_group.child_param_remove(self)

# Generic/Universal window UI, as opposed to custom UI privided by synth itself
class PluginUIUniversal(PluginUI):

    # enum for layout types
    layout_type_vertical = 0
    layout_type_horizontal = 1

    def __init__(self, plugin, group_shadow_type, layout_type):
        PluginUI.__init__(self, plugin)

        self.group_shadow_type = group_shadow_type
        self.layout_type = layout_type

        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        self.window.set_size_request(600,300)
        self.window.set_title(plugin.get_instance_name())
        self.window.set_role("zynjacku_synth_ui")

        self.window.connect("destroy", self.on_window_destroy)

        self.ui_enabled = False
        self.defered = []

    def on_window_destroy(self, window):
        if self.ui_enabled:
            self.hide()
            self.plugin.ui_off()
            self.plugin.disconnect(self.enum_disappeared_connect_id)
            self.plugin.disconnect(self.enum_appeared_connect_id)
            self.plugin.disconnect(self.float_disappeared_connect_id)
            self.plugin.disconnect(self.float_appeared_connect_id)
            self.plugin.disconnect(self.float_value_changed_id)
            self.plugin.disconnect(self.bool_disappeared_connect_id)
            self.plugin.disconnect(self.bool_appeared_connect_id)
            self.plugin.disconnect(self.group_disappeared_connect_id)
            self.plugin.disconnect(self.group_appeared_connect_id)
            self.plugin.disconnect(self.int_disappeared_connect_id)
            self.plugin.disconnect(self.int_appeared_connect_id)
        else:
            self.plugin.ui_off()

        self.emit('destroy')

    def show(self):
        '''Show synth window'''

        if not self.ui_enabled:
            self.group_appeared_connect_id = self.plugin.connect("group-appeared", self.on_group_appeared)
            self.group_disappeared_connect_id = self.plugin.connect("group-disappeared", self.on_group_disappeared)
            self.bool_appeared_connect_id = self.plugin.connect("bool-appeared", self.on_bool_appeared)
            self.bool_disappeared_connect_id = self.plugin.connect("bool-disappeared", self.on_bool_disappeared)
            self.float_appeared_connect_id = self.plugin.connect("float-appeared", self.on_float_appeared)
            self.float_disappeared_connect_id = self.plugin.connect("float-disappeared", self.on_float_disappeared)
            self.float_value_changed_id = self.plugin.connect("float-value-changed", self.on_float_value_changed)
            self.enum_appeared_connect_id = self.plugin.connect("enum-appeared", self.on_enum_appeared)
            self.enum_disappeared_connect_id = self.plugin.connect("enum-disappeared", self.on_enum_disappeared)
            self.int_appeared_connect_id = self.plugin.connect("int-appeared", self.on_int_appeared)
            self.int_disappeared_connect_id = self.plugin.connect("int-disappeared", self.on_int_disappeared)

            self.plugin.ui_on()

            self.ui_enabled = True

            for child in self.defered:
                child.parent_group.child_param_add(child)

        self.window.show_all()

    def hide(self):
        '''Hide synth window'''
        if not self.ui_enabled:
            return

        self.window.hide_all()

        self.ui_enabled = False

    def convert_hints(self, hints):
        hints_hash = {}
        for i in range(hints.get_count()):
            hints_hash[hints.get_name_at_index(i)] = hints.get_value_at_index(i)
        return hints_hash

    def on_group_appeared(self, synth, parent, group_name, hints, context):
        #print "-------------- Group \"%s\" appeared" % group_name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "group_name: %s" % group_name
        #print "hints: %s" % repr(hints)
        #print "hints count: %s" % hints.get_count()
        #for i in range(hints.get_count()):
        #    print hints.get_name_at_index(i)
        #    print hints.get_value_at_index(i)
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "context: %s" % repr(context)

        if not parent:
            return PluginUIUniversalGroupGeneric(self, parent, group_name, hints_hash, context)

        return parent.on_child_group_appeared(group_name, hints_hash, context)

    def on_group_disappeared(self, synth, obj):
        #print "-------------- Group \"%s\" disappeared" % obj.group_name
        return

    def on_bool_appeared(self, synth, parent, name, hints, value, context):
        #print "-------------- Bool \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "context: %s" % repr(context)

        return parent.on_bool_appeared(self.window, name, hints_hash, value, context)

    def on_bool_disappeared(self, synth, obj):
        #print "-------------- Bool disappeared"
        obj.remove()

    def on_float_appeared(self, synth, parent, name, hints, value, min, max, context):
        #print "-------------- Float \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "min: %s" % repr(min)
        #print "max: %s" % repr(max)
        #print "context: %s" % repr(context)

        return parent.on_float_appeared(self.window, name, hints_hash, value, min, max, context)

    def on_float_disappeared(self, synth, obj):
        #print "-------------- Float \"%s\" disappeared" % obj.parameter_name
        #print repr(self.parent_group)
        #print repr(obj)
        obj.remove()

    def on_float_value_changed(self, synth, obj, value):
        #print "-------------- Float \"%s\" value changed to %f" % (obj.parameter_name, value)
        #print repr(self.parent_group)
        #print repr(obj)
        #print "value: %s" % repr(value)
        obj.value_changed(value)

    def on_enum_appeared(self, synth, parent, name, hints, selected_value_index, valid_values, context):
        #print "-------------- Enum \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "selected value index: %s" % repr(selected_value_index)
        #print "valid values: %s" % repr(valid_values)
        #print "valid values count: %s" % valid_values.get_count()
        #for i in range(valid_values.get_count()):
        #    print valid_values.get_at_index(i)
        #print "context: %s" % repr(context)
        return parent.on_enum_appeared(self.window, name, hints_hash, selected_value_index, valid_values, context)

    def on_enum_disappeared(self, synth, obj):
        #print "-------------- Enum \"%s\" disappeared" % obj.parameter_name
        obj.remove()

    def on_int_appeared(self, synth, parent, name, hints, value, min, max, context):
        #print "-------------- Integer \"%s\" appeared" % name
        #print "synth: %s" % repr(synth)
        #print "parent: %s" % repr(parent)
        #print "name: %s" % name
        hints_hash = self.convert_hints(hints)
        #print repr(hints_hash)
        #print "value: %s" % repr(value)
        #print "min: %s" % repr(min)
        #print "max: %s" % repr(max)
        #print "context: %s" % repr(context)

        return parent.on_int_appeared(self.window, name, hints_hash, value, min, max, context)

    def on_int_disappeared(self, synth, obj):
        #print "-------------- Integer \"%s\" disappeared" % obj.parameter_name
        #print repr(self.parent_group)
        #print repr(obj)
        obj.remove()

class PluginUIUniversalGroupGeneric(PluginUIUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        PluginUIUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        if self.window.layout_type == PluginUIUniversal.layout_type_horizontal:
            self.box_params = gtk.VBox()
            self.box_top = gtk.HBox()
        else:
            self.box_params = gtk.HBox()
            self.box_top = gtk.VBox()

        self.box_top.pack_start(self.box_params, False, False)

        if parent:
            if hints.has_key(hint_uris['hidden']):
                frame = self.box_top
            else:
                frame = gtk.Frame(group_name)
                frame.set_shadow_type(self.window.group_shadow_type)
                self.frame = frame

                frame.add(self.box_top)

                if self.window.layout_type == PluginUIUniversal.layout_type_horizontal:
                    frame.set_label_align(0.5, 0.5)

            align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
            align.set_padding(0, 10, 10, 10)
            align.add(frame)
            self.top = align
            parent.child_group_add(self)
        else:
            self.scrolled_window = gtk.ScrolledWindow()
            self.scrolled_window.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)

            vp = gtk.Viewport()
            vp.add_events(gtk.gdk.ALL_EVENTS_MASK) # pass all events so Knob is working
            vp.add(self.box_top)
            self.scrolled_window.add(vp)

            self.window.statusbar = gtk.Statusbar()

            box = gtk.VBox()
            box.pack_start(self.scrolled_window, True, True)
            box.pack_start(self.window.statusbar, False)

            self.window.window.add(box)

        if window.ui_enabled:
            window.window.show_all()

        #print "Generic group \"%s\" created: %s" % (group_name, repr(self))

    def child_param_add(self, obj):
        #print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_param_remove(self, obj):
        #print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_params.remove(obj.get_top_widget())

    def child_group_add(self, obj):
        #print "child_group_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box_top.pack_start(obj.get_top_widget(), False, True)

    def child_group_remove(self, obj):
        #print "child_group_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        pass

    def on_bool_appeared(self, window, name, hints, value, context):
        parameter = PluginUIUniversalParameterBool(self.window, self, name, value, context)
        self.child_param_add(parameter)
        return parameter

    def on_float_appeared(self, window, name, hints, value, min, max, context):
        parameter = PluginUIUniversalParameterFloat(self.window, self, name, value, min, max, context)
        self.child_param_add(parameter)
        return parameter

    def on_int_appeared(self, window, name, hints, value, min, max, context):
        parameter = PluginUIUniversalParameterInt(self.window, self, name, value, min, max, context)
        self.child_param_add(parameter)
        return parameter

    def on_enum_appeared(self, window, name, hints, selected_value_index, valid_values, context):
        parameter = PluginUIUniversalParameterEnum(self.window, self, name, selected_value_index, valid_values, context)
        self.child_param_add(parameter)
        return parameter

    def get_top_widget(self):
        return self.top

    def change_display_name(self, name):
        self.frame.set_label(name)

class PluginUIUniversalGroupToggleFloat(PluginUIUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        PluginUIUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        if self.window.layout_type == PluginUIUniversal.layout_type_horizontal:
            self.box = gtk.VBox()
        else:
            self.box = gtk.HBox()

        self.float = None
        self.bool = None

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(0, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        m = re.match(r"([^:]+):(.*)", group_name)

        self.bool = PluginUIUniversalParameterBool(self.window, self, m.group(1), False, None)
        self.child_param_add(self.bool)

        self.float = PluginUIUniversalParameterFloat(self.window, self, m.group(2), 0, 0, 1, None)
        self.float.set_sensitive(False)
        self.child_param_add(self.float)

        #print "Toggle float group \"%s\" created: %s" % (group_name, repr(self))

    def child_param_add(self, obj):
        #print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)
        self.box.pack_start(obj.get_top_widget(), False, False)

        if self.window.ui_enabled:
            self.window.window.show_all()

    def child_param_remove(self, obj):
        #print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        if obj == self.float:
            self.float.set_sensitive(False)

    def get_top_widget(self):
        return self.top

    def on_bool_appeared(self, window, name, hints, value, context):
        self.bool.context = context
        return self.bool

    def on_float_appeared(self, window, name, hints, value, min, max, context):
        self.float.context = context
        self.float.set_sensitive(True)
        self.float.set(name, value, min, max)
        return self.float

class PluginUIUniversalGroupOneSubGroup(PluginUIUniversalGroup):
    def __init__(self, window, parent, group_name, hints, context):
        PluginUIUniversalGroup.__init__(self, window, parent, group_name, hints, context)

        self.box = gtk.VBox()

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)
        self.groups = []

        self.top = self.align

        self.top.connect("realize", self.on_realize)

        #print "Notebook group \"%s\" created: %s" % (group_name, repr(self))

    def on_realize(self, obj):
        #print "realize"
        self.box.pack_start(self.enum.get_top_widget())
        self.enum.connect("zynjacku-parameter-changed", self.on_changed)
        selected = self.enum.get_selection()
        for group in self.groups:
            if group.group_name == selected:
                self.box.pack_start(group.get_top_widget())
                self.selected_group = group

    def on_enum_appeared(self, window, name, hints, selected_value_index, valid_values, context):
        #print "enum appeared"
        parameter = PluginUIUniversalParameterEnum(self.window, self, name, selected_value_index, valid_values, context)
        self.enum = parameter
        return parameter

    def child_group_add(self, obj):
        #print "child_group_add %s for group \"%s\"" % (repr(obj), self.group_name)
        #self.box.pack_start(obj.get_top_widget())
        self.groups.append(obj)
        obj.change_display_name(self.group_name)

    def child_group_remove(self, obj):
        #print "child_group_remove %s for group \"%s\"" % (repr(obj), self.group_name)
        return

#    def child_param_add(self, obj):
#        print "child_param_add %s for group \"%s\"" % (repr(obj), self.group_name)

#    def child_param_remove(self, obj):
#        print "child_param_remove %s for group \"%s\"" % (repr(obj), self.group_name)

    def get_top_widget(self):
        return self.top

    def on_changed(self, adjustment):
        #print "Enum changed."
        self.box.remove(self.selected_group.get_top_widget())
        selected = self.enum.get_selection()
        for group in self.groups:
            if group.group_name == selected:
                self.box.pack_start(group.get_top_widget())
                group.get_top_widget().show_all()
                self.selected_group = group

class PluginUIUniversalParameterFloat(PluginUIUniversalParameter):
    def __init__(self, window, parent_group, name, value, min, max, context):
        PluginUIUniversalParameter.__init__(self, window, parent_group, name, context)

        self.box = gtk.VBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0, 0)
        align.add(self.label)
        self.box.pack_start(align, False, False)

        adjustment = gtk.Adjustment(value, min, max, (max - min) / 100, (max - min) / 5)

        hbox = gtk.HBox()
        self.knob = Knob()
        self.knob.set_adjustment(adjustment)
        #self.knob.set_size_request(200,-1)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.knob)
        hbox.pack_start(self.knob, True, True)
        self.spin = gtk.SpinButton(adjustment, 0.0, 2)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.spin)
        hbox.pack_start(self.spin, False, False)

        #align = gtk.Alignment(0.5, 0.5)
        #align.add(hbox)
        self.box.pack_start(hbox, True, True)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        self.adjustment = adjustment

        self.cid = adjustment.connect("value-changed", self.on_value_changed)

        self.knob.connect("button-press-event", self.on_clicked)

        self.automation = False

        #print "Float \"%s\" created: %s" % (name, repr(self))

    def on_clicked(self, widget, event):
        if event.type != gtk.gdk._2BUTTON_PRESS:
            return False

        #print "double click on %s" % self.parameter_name

        map = self.window.plugin.get_midi_cc_map(self.context)

        if not map:
            #print "new map"
            new_map = True
            map = zynjacku.MidiCcMap()
            map.point_create(0, self.adjustment.lower)
            map.point_create(127, self.adjustment.upper)
            if not self.window.plugin.set_midi_cc_map(self.context, map):
                # TODO display error message to user
                return
        else:
            #print "existing map"
            new_map = False

        ret = midiccmap(map, self.parameter_name, self.adjustment.lower, self.adjustment.upper, self.adjustment.value).run()
        if (not ret and new_map) or ret == "delete":
            #print "removing map"
            self.window.plugin.set_midi_cc_map(self.context, None)

        return True

    def get_top_widget(self):
        return self.top

    # UI change -> engine change
    def on_value_changed(self, adjustment):
        if self.automation:
            return

        #print "Float changed. \"%s\" set to %f" % (self.parameter_name, adjustment.get_value())
        self.window.plugin.float_set(self.context, adjustment.get_value())

    # engine change -> UI change
    def value_changed(self, value):
        self.automation = True
        self.adjustment.value = value
        self.automation = False

    def set_sensitive(self, sensitive):
        self.knob.set_sensitive(sensitive)
        self.spin.set_sensitive(sensitive)

    def set(self, name, value, min, max):
        self.adjustment.disconnect(self.cid)
        self.label.set_text(name)
        self.adjustment = gtk.Adjustment(value, min, max, 1, 19)
        self.spin.set_adjustment(self.adjustment)
        self.knob.set_adjustment(self.adjustment)
        self.cid = self.adjustment.connect("value-changed", self.on_value_changed)

class PluginUIUniversalParameterInt(PluginUIUniversalParameter):
    def __init__(self, window, parent_group, name, value, min, max, context):
        PluginUIUniversalParameter.__init__(self, window, parent_group, name, context)

        self.box = gtk.HBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0.5, 0.5)
        align.add(self.label)
        self.box.pack_start(align, True, True)

        adjustment = gtk.Adjustment(value, min, max, 1, 19)

        self.spin = gtk.SpinButton(adjustment, 0.0, 0)
        #align = gtk.Alignment(0.5, 0.5)
        #align.add(self.spin)
        self.box.pack_start(self.spin, True, False)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(self.box)

        self.top = self.align

        self.adjustment = adjustment

        self.cid = adjustment.connect("value-changed", self.on_value_changed)

        #print "Int \"%s\" created: %s" % (name, repr(self))

    def get_top_widget(self):
        return self.top

    def on_value_changed(self, adjustment):
        #print "Int changed. \"%s\" set to %d" % (self.parameter_name, int(adjustment.get_value()))
        self.window.plugin.int_set(self.context, int(adjustment.get_value()))

    def set_sensitive(self, sensitive):
        self.knob.set_sensitive(sensitive)
        self.spin.set_sensitive(sensitive)

    def set(self, name, value, min, max):
        self.adjustment.disconnect(self.cid)
        self.label.set_text(name)
        self.adjustment = gtk.Adjustment(value, min, max, 1, 19)
        self.spin.set_adjustment(self.adjustment)
        self.knob.set_adjustment(self.adjustment)
        self.cid = self.adjustment.connect("value-changed", self.on_value_changed)

class PluginUIUniversalParameterBool(PluginUIUniversalParameter):
    def __init__(self, window, parent_group, name, value, context):
        PluginUIUniversalParameter.__init__(self, window, parent_group, name, context)

        widget = gtk.CheckButton(name)

        widget.set_active(value)
        widget.connect("toggled", self.on_toggled)

        self.align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        self.align.set_padding(10, 10, 10, 10)
        self.align.add(widget)

    def get_top_widget(self):
        return self.align

    def on_toggled(self, widget):
        #print "Boolean toggled. \"%s\" set to \"%s\"" % (widget.get_label(), widget.get_active())
        self.window.plugin.bool_set(self.context, widget.get_active())

class PluginUIUniversalParameterEnum(PluginUIUniversalParameter):
    def __init__(self, window, parent_group, name, selected_value_index, valid_values, context):
        PluginUIUniversalParameter.__init__(self, window, parent_group, name, context)

        label = gtk.Label(name)

        self.box = gtk.VBox()

        self.label = gtk.Label(name)
        align = gtk.Alignment(0, 0)
        align.set_padding(0, 0, 10, 10)
        align.add(self.label)
        self.box.pack_start(align, True, True)

        self.liststore = gtk.ListStore(gobject.TYPE_STRING)
        self.combobox = gtk.ComboBox(self.liststore)
        self.cell = gtk.CellRendererText()
        self.combobox.pack_start(self.cell, True)
        self.combobox.add_attribute(self.cell, 'text', 0)

        for i in range(valid_values.get_count()):
            row = valid_values.get_at_index(i),
            self.liststore.append(row)

        self.combobox.set_active(selected_value_index)
        self.selected_value_index = selected_value_index;

        self.combobox.connect("changed", self.on_changed)

        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(0, 10, 10, 10)
        align.add(self.combobox)

        self.box.pack_start(align, False, False)

    def get_top_widget(self):
        return self.box

    def on_changed(self, adjustment):
        #print "Enum changed. \"%s\" set to %u" % (self.parameter_name, adjustment.get_active())
        self.selected_value_index = adjustment.get_active()
        self.window.plugin.enum_set(self.context, self.selected_value_index)
        self.emit("zynjacku-parameter-changed")

    def get_selection(self):
        return self.liststore.get(self.liststore.iter_nth_child(None, self.selected_value_index), 0)[0]

class plugin_load_progress_window:
    def __init__(self):
        self.window = gtk.Window(gtk.WINDOW_TOPLEVEL)
        #self.window.set_size_request(600,300)

        self.bar = gtk.ProgressBar()

        box = gtk.VBox()
        box.set_spacing(10)
        self.label = gtk.Label("Loading plugin ...")
        box.pack_start(self.label)
        box.pack_start(self.bar)
        align = gtk.Alignment(0.5, 0.5, 1.0, 1.0)
        align.set_padding(10, 10, 10, 10)
        align.add(box)
        self.window.add(align)

        while gtk.events_pending():
            gtk.main_iteration()

    def progress(self, name, progress, message):
        #print "Loading plugin '%s', %5.1f%% complete. %s" % (name, progress, message)
        self.bar.set_fraction(progress / 100.0)
        self.label.set_text("Loading plugin '%s'" % name)
        self.bar.set_text(message);
        while gtk.events_pending():
            gtk.main_iteration()

        while gtk.events_pending():
            gtk.main_iteration()

    def show(self, uri):
        title = "Loading plugin '%s'..." % uri
        self.window.set_title(title)
        self.bar.set_text(title);
        self.label.set_text(title)
        self.window.show_all()

    def hide(self):
        self.window.hide()

class host:
    def __init__(self, engine, client_name, preset_extension=None, preset_name=None, lash_client=None):
        #print "host constructor called."

        self.group_shadow_type = gtk.SHADOW_ETCHED_OUT
        self.layout_type = PluginUIUniversal.layout_type_horizontal

        self.engine = engine
        self.progress_connect_id = self.engine.connect("progress", self.on_plugin_progress)

        self.plugins = []

        if not self.engine.start_jack(client_name):
            print "Failed to initialize zynjacku engine"
            sys.exit(1)

        self.preset_filename = None

        self.preset_extension = preset_extension
        self.preset_name = preset_name

        self.progress_window = plugin_load_progress_window()

        if lash_client:
            # Send our client name to server
            lash_event = lash.lash_event_new_with_type(lash.LASH_Client_Name)
            lash.lash_event_set_string(lash_event, client_name)
            lash.lash_send_event(lash_client, lash_event)

            lash.lash_jack_client_name(lash_client, client_name)

            self.lash_check_events_callback_id = gobject.timeout_add(1000, self.lash_check_events)

            if not self.preset_extension:
                self.preset_extension = "xml"

        self.lash_client = lash_client

    def lash_check_events(self):
        while lash.lash_get_pending_event_count(self.lash_client):
            event = lash.lash_get_event(self.lash_client)

            #print repr(event)

            event_type = lash.lash_event_get_type(event)
            if event_type == lash.LASH_Quit:
                print "LASH ordered quit."
                self.on_quit()
                return False
            elif event_type == lash.LASH_Save_File:
                directory = lash.lash_event_get_string(event)
                print "LASH ordered to save data in directory %s" % directory
                self.preset_filename = directory + os.sep + "preset." + self.preset_extension
                self.preset_save()
                lash.lash_send_event(self.lash_client, event)
            elif event_type == lash.LASH_Restore_File:
                directory = lash.lash_event_get_string(event)
                print "LASH ordered to restore data from directory %s" % directory
                self.preset_load(directory + os.sep + "preset." + self.preset_extension)
                lash.lash_send_event(self.lash_client, event)
            else:
                print "Got unhandled LASH event, type " + str(event_type)
                return True

            #lash.lash_event_destroy(event)

        return True

    def create_plugin_ui(self, plugin, data=None):
        if not self.plugin_ui_available(plugin):
            return False

        if plugin.supports_custom_ui():
            plugin.ui_win = PluginUICustom(plugin)
        else:
            plugin.ui_win = PluginUIUniversal(plugin, self.group_shadow_type, self.layout_type)

        plugin.ui_win.destroy_connect_id = plugin.ui_win.connect("destroy", self.on_plugin_ui_window_destroyed, plugin, data)
        return True

    def on_plugin_ui_window_destroyed(self, window, plugin, data):
        return

    def plugin_ui_available(self, plugin):
        return plugin.supports_custom_ui() or plugin.supports_generic_ui()

    def new_plugin(self, uri, parameters=[], maps={}):
        plugin = zynjacku.Plugin(uri=uri)
        if not plugin.construct(self.engine):
            return False

        for parameter in parameters:
            name = parameter[0]
            value = parameter[1]
            mapid = parameter[2]

            if mapid:
                mapobj = maps[mapid]
            else:
                mapobj = None

            plugin.set_parameter(name, value, mapobj)

        plugin.uri = uri
        plugin.ui_win = None
        self.plugins.append(plugin)
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        print "Loading plugin '%s', %5.1f%% complete. %s" % (name, progress, message)

    def on_plugin_repo_tick(self, repo, progress, uri, progressbar):
        if progress == 1.0:
            progressbar.hide()
            return

        progressbar.show()
        progressbar.set_fraction(progress)
        progressbar.set_text("Checking %s" % uri);
        while gtk.events_pending():
            gtk.main_iteration()

    def on_plugin_repo_tack(self, repo, name, uri, plugin_license, author, store):
        #print "tack: %s %s %s" % (name, uri, plugin_license)
        store.append([name, uri, plugin_license, author])

    def rescan_plugins(self, store, progressbar, force):
        store.clear()
        tick = self.engine.connect("tick", self.on_plugin_repo_tick, progressbar)
        tack = self.engine.connect("tack", self.on_plugin_repo_tack, store)
        self.engine.iterate_plugins(force)
        self.engine.disconnect(tack)
        self.engine.disconnect(tick)

    def plugins_load(self, title="LV2 plugins"):
        dialog = self.glade_xml.get_widget("zynjacku_plugin_repo")
        plugin_repo_widget = self.glade_xml.get_widget("treeview_available_plugins")
        progressbar = self.glade_xml.get_widget("progressbar")

        dialog.set_title(title)

        plugin_repo_widget.get_selection().set_mode(gtk.SELECTION_MULTIPLE)

        store = gtk.ListStore(gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING)
        text_renderer = gtk.CellRendererText()

        column_name = gtk.TreeViewColumn("Name", text_renderer, text=0)
        column_uri = gtk.TreeViewColumn("URI", text_renderer, text=1)
        column_license = gtk.TreeViewColumn("License", text_renderer, text=2)
        column_author = gtk.TreeViewColumn("Author", text_renderer, text=3)

        column_name.set_sort_column_id(0)
        column_uri.set_sort_column_id(1)
        column_license.set_sort_column_id(2)
        column_author.set_sort_column_id(3)

        plugin_repo_widget.append_column(column_name)
        plugin_repo_widget.append_column(column_uri)
        plugin_repo_widget.append_column(column_license)
        plugin_repo_widget.append_column(column_author)

        plugin_repo_widget.set_model(store)
        def on_row_activated(widget, path, column):
            dialog.response(0)
        plugin_repo_widget.connect("row-activated", on_row_activated)

        dialog.show()
        self.rescan_plugins(store, progressbar, False)
        while True:
            ret = dialog.run()
            if ret == 0:
                dialog.hide()
                for path in plugin_repo_widget.get_selection().get_selected_rows()[1]:
                    self.load_plugin(store.get(store.get_iter(path), 1)[0])
                self.progress_window.hide()
                return
            elif ret == 1:
                self.rescan_plugins(store, progressbar, True)
            else:
                dialog.hide()
                return

    def on_plugin_parameter_map_point(self, mapobj, cc_value, parameter_value):
        self.xml += "%s<point cc_value='%u' parameter_value='%f' />\n" % (self.xml_indent, cc_value, parameter_value)

    def on_plugin_parameter_value(self, plugin, parameter, value, mapobj):
        if mapobj:
            self.xml_map_id += 1
            mapid = " mapid='%u'" % self.xml_map_id
        else:
            mapid = ""

        self.xml += "%s<parameter name='%s'%s>%s</parameter>\n" % (self.xml_indent, parameter, mapid, value)

        if mapobj:
            self.xml += "%s<midi_cc_map id='%u' cc_no='%d'>\n" % (self.xml_indent, self.xml_map_id, mapobj.get_cc_no())

            cbid = mapobj.connect("point-created", self.on_plugin_parameter_map_point)

            oldindent = self.xml_indent
            self.xml_indent = self.xml_indent + "  "

            mapobj.get_points()

            self.xml_indent = oldindent

            mapobj.disconnect(cbid)

            self.xml += "%s</midi_cc_map>\n" % self.xml_indent

    def get_plugins_xml(self, indent):
        self.xml = ""
        self.xml_indent = indent + "  "
        self.xml_map_id = 0
        for plugin in self.plugins:
            self.xml += "%s<plugin uri='%s'>\n" % (indent, plugin.uri)
            cbid = plugin.connect("parameter-value", self.on_plugin_parameter_value)
            plugin.get_parameters()
            plugin.disconnect(cbid)
            self.xml += "%s</plugin>\n" % indent

        return self.xml

    def load_plugin(self, uri, parameters=[], maps={}):
        pass

    def preset_load(self, filename):
        # TODO: handle exceptions

        self.preset_filename = filename

        doc = xml.dom.minidom.parse(filename)
        for plugin in doc.getElementsByTagName("plugin"):
            uri = plugin.getAttribute("uri")
            name = None
            parameters = []
            maps = {}
            for node in plugin.childNodes:
                if node.nodeType == node.ELEMENT_NODE:
                    if node.nodeName == 'parameter':
                        name = node.getAttribute("name")
                        node.normalize()
                        value = node.childNodes[0].data
                        #print "%s='%f'" % (name, value)
                        parameters.append([name, value, node.getAttribute("mapid")])
                    elif node.nodeName == 'midi_cc_map':
                        mapobj = zynjacku.MidiCcMap()
                        mapid = node.getAttribute("id")
                        mapobj.cc_no_assign(int(node.getAttribute("cc_no")))

                        for pointnode in node.childNodes:
                            if pointnode.nodeType == node.ELEMENT_NODE:
                                if pointnode.nodeName == 'point':
                                    mapobj.point_create(int(pointnode.getAttribute("cc_value")), float(pointnode.getAttribute("parameter_value")))

                        maps[mapid] = mapobj
                    else:
                        print "<%s> ?" % node.nodeName

            self.load_plugin(uri, parameters, maps)
        self.progress_window.hide()

    def setup_file_dialog_filters(self, file_dialog):
        # Create and add the filter
        filter = gtk.FileFilter()
        pattern = '*.' + self.preset_extension
        if self.preset_name:
            filter.set_name(self.preset_name + ' (' + pattern + ')')
        else:
            filter.set_name(pattern)
        filter.add_pattern(pattern)
        file_dialog.add_filter(filter)

        # Create and add the 'all files' filter
        filter = gtk.FileFilter()
        filter.set_name("All files")
        filter.add_pattern("*")
        file_dialog.add_filter(filter)


    def preset_load_ask(self):
        dialog_buttons = (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK)
        if self.preset_name:
            title = "Load " + self.preset_name
        else:
            title = "Load preset"
        file_dialog = gtk.FileChooserDialog(title=title, action=gtk.FILE_CHOOSER_ACTION_OPEN, buttons=dialog_buttons)

        self.setup_file_dialog_filters(file_dialog)

        # Init the return value
        filename = ""

        if file_dialog.run() == gtk.RESPONSE_OK:
            filename = file_dialog.get_filename()
        file_dialog.destroy()

        if not filename:
            return

        self.preset_load(filename)

    def preset_get_pre_plugins_xml(self):
        pass

    def preset_get_post_plugins_xml(self):
        pass

    def preset_save(self):
        # TODO: check for overwrite and handle exceptions
        store = open(self.preset_filename, 'w')

        xml = "<?xml version=\"1.0\"?>\n"
        if self.preset_name:
            xml += "<!-- This is a " + self.preset_name + " preset file -->\n"
        xml += "<!-- saved on " + time.ctime() + " -->\n"
        xml += self.preset_get_pre_plugins_xml()
        xml += self.get_plugins_xml("    ")
        xml += self.preset_get_post_plugins_xml()

        store.write(xml)
        store.close()

    def preset_save_ask(self):
        dialog_buttons = (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_SAVE, gtk.RESPONSE_OK)
        if self.preset_name:
            title = "Save " + self.preset_name
        else:
            title = "Save preset"
        file_dialog = gtk.FileChooserDialog(title=title, action=gtk.FILE_CHOOSER_ACTION_SAVE, buttons=dialog_buttons)

        # set the filename
        if self.preset_filename:
            file_dialog.set_current_name(self.preset_filename)

        self.setup_file_dialog_filters(file_dialog)

        # Init the return value
        filename = ""

        if file_dialog.run() == gtk.RESPONSE_OK:
            filename = file_dialog.get_filename()
        file_dialog.destroy()

        # We have a path, ensure the proper extension
        filename, extension = os.path.splitext(filename)
        filename += "." + self.preset_extension

        self.preset_filename = filename

        self.preset_save()

    def clear_plugins(self):
        for plugin in self.plugins:
            plugin.destruct()
        self.plugins = []

    def __del__(self):
        #print "host destructor called."
        self.clear_plugins()

        self.engine.stop_jack()

    def ui_run(self):
        self.engine.ui_run()
        return True

    def run(self):
        ui_run_callback_id = gobject.timeout_add(40, self.ui_run)

        gtk.main()
        gobject.source_remove(ui_run_callback_id)
        if self.lash_client:
            #print "removing lash handler, host object refcount is %u" % sys.getrefcount(self)
            gobject.source_remove(self.lash_check_events_callback_id)
            #print "removed lash handler, host object refcount is %u" % sys.getrefcount(self)

        for plugin in self.plugins:
            if plugin.ui_win:
                plugin.ui_win.disconnect(plugin.ui_win.destroy_connect_id) # signal connection holds reference to plugin object...

        self.engine.disconnect(self.progress_connect_id)

    def on_test(self, obj1, obj2):
        print "on_test() called !!!!!!!!!!!!!!!!!!"
        print repr(obj1)
        print repr(obj2)

class ZynjackuHost(host):
    def __init__(self, client_name, preset_extension=None, preset_name=None, lash_client=None):
        #print "ZynjackuHost constructor called."

        host.__init__(self, zynjacku.Engine(), client_name, preset_extension, preset_name, lash_client)

class ZynjackuHostMulti(ZynjackuHost):
    def __init__(self, data_dir, glade_xml, client_name, the_license, uris, lash_client):
        #print "ZynjackuHostMulti constructor called."
        ZynjackuHost.__init__(self, client_name, "zynjacku", "synth stack", lash_client)
        
        self.data_dir = data_dir
        self.glade_xml = glade_xml

        self.main_window = glade_xml.get_widget("zynjacku_main")
        self.main_window.set_title(client_name)

        self.statusbar = self.glade_xml.get_widget("statusbar")

        self.hbox_menubar = glade_xml.get_widget("hbox_menubar")
        self.midi_led = midi_led()
        self.midi_led_frame = gtk.Frame()
        self.midi_led_frame.set_shadow_type(gtk.SHADOW_OUT)
        self.midi_led_frame.add(self.midi_led);
        self.hbox_menubar.pack_start(self.midi_led_frame, False, False)

	# Create our dictionary and connect it
        dic = {"quit" : self.on_quit,
               "about" : self.on_about,
               "preset_load" : self.on_preset_load,
               "preset_save_as" : self.on_preset_save_as,
               "synth_load" : self.on_synth_load,
               "synth_clear" : self.on_synth_clear,
               }

        self.signal_ids = []
        for k, v in dic.items():
            w = glade_xml.get_widget(k)
            if not w:
                print "failed to get glade widget '%s'" % k
                continue
            self.signal_ids.append([w, w.connect("activate", v)])

        self.the_license = the_license

        self.synths_widget = glade_xml.get_widget("treeview_synths")

        self.store = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_PYOBJECT)
        text_renderer = gtk.CellRendererText()
        self.toggle_renderer = gtk.CellRendererToggle()
        self.toggle_renderer.set_property('activatable', True)

        column_ui_visible = gtk.TreeViewColumn("UI", self.toggle_renderer)
        column_ui_visible.add_attribute(self.toggle_renderer, "active", 0)
        column_instance = gtk.TreeViewColumn("Instance", text_renderer, text=1)
        column_name = gtk.TreeViewColumn("Name", text_renderer, text=2)
        #column_uri = gtk.TreeViewColumn("URI", text_renderer, text=3)

        self.synths_widget.append_column(column_ui_visible)
        self.synths_widget.append_column(column_instance)
        self.synths_widget.append_column(column_name)
        #self.synths_widget.append_column(column_uri)

        self.synths_widget.set_model(self.store)

        self.main_window.show_all()
        self.signal_ids.append([self.main_window, self.main_window.connect("destroy", self.on_quit)])

        if len(uris) == 1 and uris[0][-9:] == ".zynjacku":
            self.preset_load(uris[0])
        else:
            for uri in uris:
                self.load_plugin(uri)
            self.progress_window.hide()

    def on_quit(self, window=None):
        #print "ZynjackuHostMulti::on_quit() called"
        for cid in self.signal_ids:
            #print "disconnecting %u" % cid[1]
            cid[0].disconnect(cid[1])
            #print "host object refcount is %u" % sys.getrefcount(self)
        gtk.main_quit()

    def __del__(self):
        #print "ZynjackuHostMulti destructor called."

        self.store.clear()

        ZynjackuHost.__del__(self)

    def new_plugin(self, uri, parameters=[], maps={}):
        self.progress_window.show(uri)
        plugin = ZynjackuHost.new_plugin(self, uri, parameters, maps)
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        self.progress_window.progress(name, progress, message)

    def update_midi_led(self):
        self.midi_led.set(self.engine.get_midi_activity())
        return True

    def load_plugin(self, uri, parameters=[], maps={}):
        statusbar_context_id = self.statusbar.get_context_id("loading plugin")
        statusbar_id = self.statusbar.push(statusbar_context_id, "Loading %s" % uri)
        while gtk.events_pending():
            gtk.main_iteration()
        self.statusbar.pop(statusbar_id)
        synth = self.new_plugin(uri, parameters, maps)
        if not synth:
            self.statusbar.push(statusbar_context_id, "Failed to construct %s" % uri)
        else:
            row = False, synth.get_instance_name(), synth.get_name(), synth.get_uri(), synth
            self.store.append(row)
            self.statusbar.remove(statusbar_context_id, statusbar_id)

    def run(self):
        toggled_connect_id = self.toggle_renderer.connect('toggled', self.on_ui_visible_toggled, self.store)

        update_midi_led_callback_id = gobject.timeout_add(100, self.update_midi_led)

        ZynjackuHost.run(self)

        gobject.source_remove(update_midi_led_callback_id)

        self.toggle_renderer.disconnect(toggled_connect_id)

    def on_plugin_ui_window_destroyed(self, window, synth, row):
        synth.ui_win.disconnect(synth.ui_win.destroy_connect_id) # signal connection holds reference to synth object...
        synth.ui_win = None
        row[0] = False

    def on_ui_visible_toggled(self, cell, path, model):
        #print "on_ui_visible_toggled() called."
        if model[path][0]:
            model[path][4].ui_win.hide()
            model[path][0] = False
        else:
            if not model[path][4].ui_win:
                if not self.create_plugin_ui(model[path][4], model[path]):
                    return

            statusbar_context_id = self.statusbar.get_context_id("loading plugin UI")
            if model[path][4].ui_win.show():
                model[path][0] = True
            else:
                self.statusbar.push(statusbar_context_id, "Failed to show synth UI")

    def on_about(self, widget):
        about = gtk.AboutDialog()
        about.set_transient_for(self.main_window)
        about.set_name("zynjacku")
        if zynjacku.zynjacku_get_version() == "dev":
            about.set_comments("(development snapshot)")
        else:
            about.set_version(zynjacku.zynjacku_get_version())
        about.set_license(self.the_license)
        about.set_website("http://home.gna.org/zynjacku/")
        about.set_authors(["Nedko Arnaudov"])
        about.set_artists(["Thorsten Wilms"])
        about.set_logo(gtk.gdk.pixbuf_new_from_file("%s/logo.png" % self.data_dir))
        about.show()
        about.run()
        about.hide()

    def on_preset_load(self, widget):
        self.preset_load_ask()

    def preset_get_pre_plugins_xml(self):
        xml = "<zynjacku>\n"
        xml += "  <plugins>\n"
        return xml

    def preset_get_post_plugins_xml(self):
        xml = "  </plugins>\n"
        xml += "</zynjacku>\n"
        return xml

    def on_preset_save_as(self, widget):
        self.preset_save_ask()

    def on_synth_load(self, widget):
        self.plugins_load("LV2 synth plugins")

    def on_synth_clear(self, widget):
        self.store.clear();
        self.clear_plugins()

class ZynjackuHostOne(ZynjackuHost):
    def __init__(self, glade_xml, client_name, uri):
        #print "ZynjackuHostOne constructor called."
        ZynjackuHost.__init__(self, client_name)

        self.plugin = self.new_plugin(uri)
        if not self.plugin:
            print"Failed to construct %s" % uri
            return

        if not ZynjackuHost.plugin_ui_available(self, self.plugin):
            print"Synth window not available"
            return

        if not ZynjackuHost.create_plugin_ui(self, self.plugin):
            print"Failed to create synth window"
            return

    def new_plugin(self, uri, parameters=[], maps={}):
        self.progress_window.show(uri)
        plugin = ZynjackuHost.new_plugin(self, uri, parameters, maps)
        self.progress_window.hide()
        return plugin

    def on_plugin_progress(self, engine, name, progress, message):
        self.progress_window.progress(name, progress, message)

    def on_plugin_ui_window_destroyed(self, window, synth, row):
        gtk.main_quit()

    def run(self):
        if not self.plugin:
            return

        self.plugin.ui_win.show()
        ZynjackuHost.run(self)

    def __del__(self):
        #print "ZynjackuHostOne destructor called."

        ZynjackuHost.__del__(self)

def file_setup():
    data_dir = os.path.dirname(sys.argv[0])

    # since ppl tend to run "python zynjacku.py", lets assume that it is in current directory
    # "python ./zynjacku.py" and "./zynjacku.py" will work anyway.
    if not data_dir:
        data_dir = "."

    glade_file = data_dir + os.sep + "zynjacku.glade"

    if not os.path.isfile(glade_file):
        data_dir = data_dir + os.sep + ".." + os.sep + "share"+ os.sep + "zynjacku"
        glade_file = data_dir + os.sep + "zynjacku.glade"

    #print 'data dir is "%s"' % data_dir
    #print 'Loading glade from "%s"' % glade_file

    the_license = file(data_dir + os.sep + "gpl.txt").read()

    glade_xml = gtk.glade.XML(glade_file)

    return data_dir, glade_xml, the_license

def register_types():
    gobject.signal_new("zynjacku-parameter-changed", PluginUIUniversalParameter, gobject.SIGNAL_RUN_FIRST | gobject.SIGNAL_ACTION, gobject.TYPE_NONE, [])
    gobject.type_register(PluginUI)
    gobject.type_register(PluginUIUniversal)
    gobject.type_register(PluginUIUniversalGroupGeneric)
    gobject.type_register(PluginUIUniversalGroupToggleFloat)
    gobject.type_register(PluginUIUniversalParameterFloat)
    gobject.type_register(PluginUIUniversalParameterBool)

def main():
    data_dir, glade_xml, the_license = file_setup()

    register_types()

    client_name = "zynjacku"

    if lash:                        # If LASH python bindings are available
        # sys.argv is modified by this call
        lash_client = lash.init(sys.argv, "zynjacku", lash.LASH_Config_File)
    else:
        lash_client = None

    # TODO: generic argument processing goes here

    # Yeah , this sounds stupid, we connected earlier, but we dont want to show this if we got --help option
    # This issue should be fixed in pylash, there is a reason for having two functions for initialization after all
    if lash_client:
        print "Successfully connected to LASH server at " +  lash.lash_get_server_name(lash_client)

    if len(sys.argv) == 2 and sys.argv[1][-9:] != ".zynjacku":
        host = ZynjackuHostOne(glade_xml, client_name, sys.argv[1])
    else:
        host = ZynjackuHostMulti(data_dir, glade_xml, client_name, the_license, sys.argv[1:], lash_client)

    host.run()

    #print "stone after host.run(), host object refcount is %u" % sys.getrefcount(host)
    sys.stdout.flush()
    sys.stderr.flush()

if __name__ == '__main__':
    main()
