# text.tcl --
#
# This demonstration script creates a text widget that describes
# the basic editing functions.
#
# RCS: @(#) $Id: text.tcl,v 1.8 2007/12/13 15:27:07 dgp Exp $

if {![info exists widgetDemo]} {
    error "This script should be run from the \"widget\" demo."
}

package require Tk

set w .text
catch {destroy $w}
toplevel $w
wm title $w "Text Demonstration - Basic Facilities"
wm iconname $w "text"
positionWindow $w

## See Code / Dismiss buttons
set btns [addSeeDismiss $w.buttons $w]
pack $btns -side bottom -fill x

text $w.text -yscrollcommand [list $w.scroll set] -setgrid 1 \
	-height 30 -undo 1 -autosep 1
scrollbar $w.scroll -command [list $w.text yview]
pack $w.scroll -side right -fill y
pack $w.text -expand yes -fill both
$w.text insert 0.0 \
{This window is a text widget.  It displays one or more lines of text
and allows you to edit the text.  Here is a summary of the things you
can do to a text widget:

1. Scrolling. Use the scrollbar to adjust the view in the text window.

2. Scanning. Press mouse button 2 in the text window and drag up or down.
This will drag the text at high speed to allow you to scan its contents.

3. Insert text. Press mouse button 1 to set the insertion cursor, then
type text.  What you type will be added to the widget.

4. Select. Press mouse button 1 and drag to select a range of characters.
Once you've released the button, you can adjust the selection by pressing
button 1 with the shift key down.  This will reset the end of the
selection nearest the mouse cursor and you can drag that end of the
selection by dragging the mouse before releasing the mouse button.
You can double-click to select whole words or triple-click to select
whole lines.

5. Delete and replace. To delete text, select the characters you'd like
to delete and type Backspace or Delete.  Alternatively, you can type new
text, in which case it will replace the selected text.

6. Copy the selection. To copy the selection into this window, select
what you want to copy (either here or in another application), then
click button 2 to copy the selection to the point of the mouse cursor.

7. Edit.  Text widgets support the standard Motif editing characters
plus many Emacs editing characters.  Backspace and Control-h erase the
character to the left of the insertion cursor.  Delete and Control-d
erase the character to the right of the insertion cursor.  Meta-backspace
deletes the word to the left of the insertion cursor, and Meta-d deletes
the word to the right of the insertion cursor.  Control-k deletes from
the insertion cursor to the end of the line, or it deletes the newline
character if that is the only thing left on the line.  Control-o opens
a new line by inserting a newline character to the right of the insertion
cursor.  Control-t transposes the two characters on either side of the
insertion cursor.  Control-z undoes the last editing action performed,
and }

switch $tcl_platform(platform) {
    "unix" {
	$w.text insert end "Control-Shift-z"
    }
    "windows" {
	$w.text insert end "Control-y"
    }
}

$w.text insert end { redoes undone edits.

7. Resize the window.  This widget has been configured with the "setGrid"
option on, so that if you resize the window it will always resize to an
even number of characters high and wide.  Also, if you make the window
narrow you can see that long lines automatically wrap around onto
additional lines so that all the information is always visible.}
$w.text mark set insert 0.0
