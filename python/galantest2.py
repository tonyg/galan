#!/usr/bin/env python

import sys
sys.setdlopenflags(0x102)

import galan

galan.init_all()

gc = galan.PyGenClasses()

for i in gc:
	print i

o=galan.PyGen( gc["audio_out"], "out" )
s=galan.PyGen( gc["vco"], "osc" )

s.link( 1, 0, o, 0 )

galan.mainloop()
