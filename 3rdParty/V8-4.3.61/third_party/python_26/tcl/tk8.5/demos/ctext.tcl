# ctext.tcl --
#
# This demonstration script creates a canvas widget with a text
# item that can be edited and reconfigured in various ways.
#
# RCS: @(#) $Id: ctext.tcl,v 1.5 2004/12/21 11:56:35 dkf Exp $

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

package require Tk

set w .ctext
catch {destroy $w}
toplevel $w
wm title $w "Canvas Text Demonstration"
wm iconname $w "Text"
positionWindow $w
set c $w.c

label $w.msg -font $font -wraplength 5i -justify left -text "This window displays a string of text to demonstrate the text facilities of canvas widgets.  You can click in the boxes to adjust the position of the text relative to its positioning point or change its justification.  The text also supports the following simple bindings for editing:
  1. You can point, click, and type.
  2. You can also select with button 1.
  3. You can copy the selection to the mouse position with button 2.
  4. Backspace and Control+h delete the selection if there is one;
     otherwise they delete the character just before the insertion cursor.
  5. Delete deletes the selection if there is one; otherwise it deletes
     the character just after the insertion cursor."
pack $w.msg -side top

## See Code / Dismiss buttons
set btns [addSeeDismiss $w.buttons $w]
pack $btns -side bottom -fill x

canvas $c -relief flat -borderwidth 0 -width 500 -height 350
pack $w.c -side top -expand yes -fill both

set textFont {Helvetica 24}

$c create rectangle 245 195 255 205 -outline black -fill red

# First, create the text item and give it bindings so it can be edited.

$c addtag text withtag [$c create text 250 200 -text "This is just a string of text to demonstrate the text facilities of canvas widgets. Bindings have been been defined to support editing (see above)." -width 440 -anchor n -font {Helvetica 24} -justify left]
$c bind text <1> "textB1Press $c %x %y"
$c bind text <B1-Motion> "textB1Move $c %x %y"
$c bind text <Shift-1> "$c select adjust current @%x,%y"
$c bind text <Shift-B1-Motion> "textB1Move $c %x %y"
$c bind text <KeyPress> "textInsert $c %A"
$c bind text <Return> "textInsert $c \\n"
$c bind text <Control-h> "textBs $c"
$c bind text <BackSpace> "textBs $c"
$c bind text <Delete> "textDel $c"
$c bind text <2> "textPaste $c @%x,%y" 

# Next, create some items that allow the text's anchor position
# to be edited.

proc mkTextConfig {w x y option value color} {
    set item [$w create rect $x $y [expr {$x+30}] [expr {$y+30}] \
	    -outline black -fill $color -width 1]
    $w bind $item <1> "$w itemconf text $option $value"
    $w addtag config withtag $item
}

set x 50
set y 50
set color LightSkyBlue1
mkTextConfig $c $x $y -anchor se $color
mkTextConfig $c [expr {$x+30}] [expr {$y   }] -anchor s      $color
mkTextConfig $c [expr {$x+60}] [expr {$y   }] -anchor sw     $color
mkTextConfig $c [expr {$x   }] [expr {$y+30}] -anchor e      $color
mkTextConfig $c [expr {$x+30}] [expr {$y+30}] -anchor center $color
mkTextConfig $c [expr {$x+60}] [expr {$y+30}] -anchor w      $color
mkTextConfig $c [expr {$x   }] [expr {$y+60}] -anchor ne     $color
mkTextConfig $c [expr {$x+30}] [expr {$y+60}] -anchor n      $color
mkTextConfig $c [expr {$x+60}] [expr {$y+60}] -anchor nw     $color
set item [$c create rect \
	[expr {$x+40}] [expr {$y+40}] [expr {$x+50}] [expr {$y+50}] \
	-outline black -fill red]
$c bind $item <1> "$c itemconf text -anchor center"
$c create text [expr {$x+45}] [expr {$y-5}] \
	-text {Text Position}  -anchor s  -font {Times 24}  -fill brown

# Lastly, create some items that allow the text's justification to be
# changed.

set x 350
set y 50
set color SeaGreen2
mkTextConfig $c $x $y -justify left $color
mkTextConfig $c [expr {$x+30}] $y -justify center $color
mkTextConfig $c [expr {$x+60}] $y -justify right $color
$c create text [expr {$x+45}] [expr {$y-5}] \
	-text {Justification}  -anchor s  -font {Times 24}  -fill brown

$c bind config <Enter> "textEnter $c"
$c bind config <Leave> "$c itemconf current -fill \$textConfigFill"

set textConfigFill {}

proc textEnter {w} {
    global textConfigFill
    set textConfigFill [lindex [$w itemconfig current -fill] 4]
    $w itemconfig current -fill black
}

proc textInsert {w string} {
    if {$string == ""} {
	return
    }
    catch {$w dchars text sel.first sel.last}
    $w insert text insert $string
}

proc textPaste {w pos} {
    catch {
	$w insert text $pos [selection get]
    }
}

proc textB1Press {w x y} {
    $w icursor current @$x,$y
    $w focus current
    focus $w
    $w select from current @$x,$y
}

proc textB1Move {w x y} {
    $w select to current @$x,$y
}

proc textBs {w} {
    if {![catch {$w dchars text sel.first sel.last}]} {
	return
    }
    set char [expr {[$w index text insert] - 1}]
    if {$char >= 0} {$w dchar text $char}
}

proc textDel {w} {
    if {![catch {$w dchars text sel.first sel.last}]} {
	return
    }
    $w dchars text insert
}
