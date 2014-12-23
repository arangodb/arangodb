# label.tcl --
#
# This demonstration script creates a toplevel window containing
# several label widgets.
#
# RCS: @(#) $Id: label.tcl,v 1.6 2004/12/21 11:56:35 dkf Exp $

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

package require Tk

set w .label
catch {destroy $w}
toplevel $w
wm title $w "Label Demonstration"
wm iconname $w "label"
positionWindow $w

label $w.msg -font $font -wraplength 4i -justify left -text "Five labels are displayed below: three textual ones on the left, and a bitmap label and a text label on the right.  Labels are pretty boring because you can't do anything with them."
pack $w.msg -side top

## See Code / Dismiss buttons
set btns [addSeeDismiss $w.buttons $w]
pack $btns -side bottom -fill x

frame $w.left
frame $w.right
pack $w.left $w.right -side left -expand yes -padx 10 -pady 10 -fill both

label $w.left.l1 -text "First label"
label $w.left.l2 -text "Second label, raised" -relief raised
label $w.left.l3 -text "Third label, sunken" -relief sunken
pack $w.left.l1 $w.left.l2 $w.left.l3 -side top -expand yes -pady 2 -anchor w

# Main widget program sets variable tk_demoDirectory
label $w.right.bitmap -borderwidth 2 -relief sunken \
	-bitmap @[file join $tk_demoDirectory images face.xbm]
label $w.right.caption -text "Tcl/Tk Proprietor"
pack $w.right.bitmap $w.right.caption -side top
