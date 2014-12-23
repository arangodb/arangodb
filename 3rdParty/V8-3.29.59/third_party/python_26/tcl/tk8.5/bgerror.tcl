# bgerror.tcl --
#
#	Implementation of the bgerror procedure.  It posts a dialog box with
#	the error message and gives the user a chance to see a more detailed
#	stack trace, and possible do something more interesting with that
#	trace (like save it to a log).  This is adapted from work done by
#	Donal K. Fellows.
#
# Copyright (c) 1998-2000 by Ajuba Solutions.
# Copyright (c) 2007 by ActiveState Software Inc.
# Copyright (c) 2007 Daniel A. Steffen <das@users.sourceforge.net>
# 
# RCS: @(#) $Id: bgerror.tcl,v 1.38 2007/12/13 15:26:26 dgp Exp $
# $Id: bgerror.tcl,v 1.38 2007/12/13 15:26:26 dgp Exp $

namespace eval ::tk::dialog::error {
    namespace import -force ::tk::msgcat::*
    namespace export bgerror
    option add *ErrorDialog.function.text [mc "Save To Log"] \
	widgetDefault
    option add *ErrorDialog.function.command [namespace code SaveToLog]
    option add *ErrorDialog*Label.font TkCaptionFont widgetDefault
    if {[tk windowingsystem] eq "aqua"} {
	option add *ErrorDialog*background systemAlertBackgroundActive \
		widgetDefault
	option add *ErrorDialog*info.text.background white widgetDefault
	option add *ErrorDialog*Button.highlightBackground \
		systemAlertBackgroundActive widgetDefault
    }
}

proc ::tk::dialog::error::Return {} {
    variable button

    .bgerrorDialog.ok configure -state active -relief sunken
    update idletasks
    after 100
    set button 0
}

proc ::tk::dialog::error::Details {} {
    set w .bgerrorDialog
    set caption [option get $w.function text {}]
    set command [option get $w.function command {}]
    if { ($caption eq "") || ($command eq "") } {
	grid forget $w.function
    }
    lappend command [$w.top.info.text get 1.0 end-1c]
    $w.function configure -text $caption -command $command
    grid $w.top.info - -sticky nsew -padx 3m -pady 3m
}

proc ::tk::dialog::error::SaveToLog {text} {
    if { $::tcl_platform(platform) eq "windows" } {
	set allFiles *.*
    } else {
	set allFiles *
    }
    set types [list	\
	    [list [mc "Log Files"] .log]	\
	    [list [mc "Text Files"] .txt]	\
	    [list [mc "All Files"] $allFiles] \
	    ]
    set filename [tk_getSaveFile -title [mc "Select Log File"] \
	    -filetypes $types -defaultextension .log -parent .bgerrorDialog]
    if {![string length $filename]} {
	return
    }
    set f [open $filename w]
    puts -nonewline $f $text
    close $f
}

proc ::tk::dialog::error::Destroy {w} {
    if {$w eq ".bgerrorDialog"} {
	variable button
	set button -1
    }
}

# ::tk::dialog::error::bgerror --
# This is the default version of bgerror.
# It tries to execute tkerror, if that fails it posts a dialog box containing
# the error message and gives the user a chance to ask to see a stack
# trace.
# Arguments:
# err -			The error message.

proc ::tk::dialog::error::bgerror err {
    global errorInfo tcl_platform
    variable button

    set info $errorInfo

    set ret [catch {::tkerror $err} msg];
    if {$ret != 1} {return -code $ret $msg}

    # Ok the application's tkerror either failed or was not found
    # we use the default dialog then :
    set windowingsystem [tk windowingsystem]
    if {$windowingsystem eq "aqua"} {
	set ok [mc Ok]
    } else {
	set ok [mc OK]
    }

    # Truncate the message if it is too wide (>maxLine characters) or
    # too tall (>4 lines).  Truncation occurs at the first point at
    # which one of those conditions is met.
    set displayedErr ""
    set lines 0
    set maxLine 45
    foreach line [split $err \n] {
	if { [string length $line] > $maxLine } {
	    append displayedErr "[string range $line 0 [expr {$maxLine-3}]]..."
	    break
	}
	if { $lines > 4 } {
	    append displayedErr "..."
	    break
	} else {
	    append displayedErr "${line}\n"
	}
	incr lines
    }

    set title [mc "Application Error"]
    set text [mc "Error: %1\$s" $displayedErr]
    set buttons [list ok $ok dismiss [mc "Skip Messages"] \
		     function [mc "Details >>"]]

    # 1. Create the top-level window and divide it into top
    # and bottom parts.

    set dlg .bgerrorDialog
    destroy $dlg
    toplevel $dlg -class ErrorDialog
    wm withdraw $dlg
    wm title $dlg $title
    wm iconname $dlg ErrorDialog
    wm protocol $dlg WM_DELETE_WINDOW { }

    if {$windowingsystem eq "aqua"} {
	::tk::unsupported::MacWindowStyle style $dlg moveableAlert {}
    }

    frame $dlg.bot
    frame $dlg.top
    if {$windowingsystem eq "x11"} {
	$dlg.bot configure -relief raised -bd 1
	$dlg.top configure -relief raised -bd 1
    }
    pack $dlg.bot -side bottom -fill both
    pack $dlg.top -side top -fill both -expand 1

    set W [frame $dlg.top.info]
    text $W.text -setgrid true -height 10 -wrap char \
	-yscrollcommand [list $W.scroll set]
    if {$windowingsystem ne "aqua"} {
	$W.text configure -width 40
    }

    scrollbar $W.scroll -command [list $W.text yview]
    pack $W.scroll -side right -fill y
    pack $W.text -side left -expand yes -fill both
    $W.text insert 0.0 "$err\n$info"
    $W.text mark set insert 0.0
    bind $W.text <ButtonPress-1> { focus %W }
    $W.text configure -state disabled

    # 2. Fill the top part with bitmap and message

    # Max-width of message is the width of the screen...
    set wrapwidth [winfo screenwidth $dlg]
    # ...minus the width of the icon, padding and a fudge factor for
    # the window manager decorations and aesthetics.
    set wrapwidth [expr {$wrapwidth-60-[winfo pixels $dlg 9m]}]
    label $dlg.msg -justify left -text $text -wraplength $wrapwidth
    if {$windowingsystem eq "aqua"} {
	# On the Macintosh, use the stop bitmap
	label $dlg.bitmap -bitmap stop
    } else {
	# On other platforms, make the error icon
	canvas $dlg.bitmap -width 32 -height 32 -highlightthickness 0
	$dlg.bitmap create oval 0 0 31 31 -fill red -outline black
	$dlg.bitmap create line 9 9 23 23 -fill white -width 4
	$dlg.bitmap create line 9 23 23 9 -fill white -width 4
    }
    grid $dlg.bitmap $dlg.msg -in $dlg.top -row 0 -padx 3m -pady 3m
    grid configure	 $dlg.msg -sticky nsw -padx {0 3m}
    grid rowconfigure	 $dlg.top 1 -weight 1
    grid columnconfigure $dlg.top 1 -weight 1

    # 3. Create a row of buttons at the bottom of the dialog.

    set i 0
    foreach {name caption} $buttons {
	button $dlg.$name -text $caption -default normal \
	    -command [namespace code [list set button $i]]
	grid $dlg.$name -in $dlg.bot -column $i -row 0 -sticky ew -padx 10
	grid columnconfigure $dlg.bot $i -weight 1
	# We boost the size of some Mac buttons for l&f
	if {$windowingsystem eq "aqua"} {
	    if {($name eq "ok") || ($name eq "dismiss")} {
		grid columnconfigure $dlg.bot $i -minsize 90
	    }
	    grid configure $dlg.$name -pady 7
	}
	incr i
    }
    # The "OK" button is the default for this dialog.
    $dlg.ok configure -default active

    bind $dlg <Return>	[namespace code Return]
    bind $dlg <Destroy>	[namespace code [list Destroy %W]]
    $dlg.function configure -command [namespace code Details]

    # 6. Place the window (centered in the display) and deiconify it.

    ::tk::PlaceWindow $dlg

    # 7. Ensure that we are topmost.

    raise $dlg
    if {$tcl_platform(platform) eq "windows"} {
	# Place it topmost if we aren't at the top of the stacking
	# order to ensure that it's seen
	if {[lindex [wm stackorder .] end] ne "$dlg"} {
	    wm attributes $dlg -topmost 1
	}
    }

    # 8. Set a grab and claim the focus too.

    ::tk::SetFocusGrab $dlg $dlg.ok

    # 9. Wait for the user to respond, then restore the focus and
    # return the index of the selected button.  Restore the focus
    # before deleting the window, since otherwise the window manager
    # may take the focus away so we can't redirect it.  Finally,
    # restore any grab that was in effect.

    vwait [namespace which -variable button]
    set copy $button; # Save a copy...

    ::tk::RestoreFocusGrab $dlg $dlg.ok destroy

    if {$copy == 1} {
	return -code break
    }
}

namespace eval :: {
    # Fool the indexer
    proc bgerror err {}
    rename bgerror {}
    namespace import ::tk::dialog::error::bgerror
}
