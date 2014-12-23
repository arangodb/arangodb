#
# $Id: sizegrip.tcl,v 1.1 2006/10/31 01:42:27 hobbs Exp $
#
# Ttk widget set -- sizegrip widget bindings.
#
# Dragging a sizegrip widget resizes the containing toplevel.
#
# NOTE: the sizegrip widget must be in the lower right hand corner.
#

option add *TSizegrip.cursor $::ttk::Cursors(seresize)

namespace eval ttk::sizegrip {
    variable State
    array set State {
	pressed 	0
	pressX 		0
	pressY 		0
	width 		0
	height 		0
	widthInc	1
	heightInc	1
	toplevel 	{}
    }
}

bind TSizegrip <ButtonPress-1> 		{ ttk::sizegrip::Press	%W %X %Y }
bind TSizegrip <B1-Motion> 		{ ttk::sizegrip::Drag 	%W %X %Y }
bind TSizegrip <ButtonRelease-1> 	{ ttk::sizegrip::Release %W %X %Y }

proc ttk::sizegrip::Press {W X Y} {
    variable State

    set top [winfo toplevel $W]

    # Sanity-checks:
    #	If a negative X or Y position was specified for [wm geometry],
    #   just bail out -- there's no way to handle this cleanly.
    #
    if {[scan [wm geometry $top] "%dx%d+%d+%d" width height _x _y] != 4} {
	return;
    }

    # Account for gridded geometry:
    #
    set grid [wm grid $top]
    if {[llength $grid]} {
	set State(widthInc) [lindex $grid 2]
	set State(heightInc) [lindex $grid 3]
    } else {
	set State(widthInc) [set State(heightInc) 1]
    }

    set State(toplevel) $top
    set State(pressX) $X
    set State(pressY) $Y
    set State(width)  $width
    set State(height) $height
    set State(pressed) 1
}

proc ttk::sizegrip::Drag {W X Y} {
    variable State
    if {!$State(pressed)} { return }
    set w [expr {$State(width)  + ($X - $State(pressX))/$State(widthInc)}]
    set h [expr {$State(height) + ($Y - $State(pressY))/$State(heightInc)}]
    if {$w <= 0} { set w 1 }
    if {$h <= 0} { set h 1 }
    wm geometry $State(toplevel) ${w}x${h}
}

proc ttk::sizegrip::Release {W X Y} {
    variable State
    set State(pressed) 0
}

#*EOF*
