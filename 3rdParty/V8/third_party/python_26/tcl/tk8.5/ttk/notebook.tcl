#
# $Id: notebook.tcl,v 1.4 2007/02/24 09:15:07 das Exp $
#
# Bindings for TNotebook widget
#

namespace eval ttk::notebook {
    variable TLNotebooks ;# See enableTraversal
}

bind TNotebook <ButtonPress-1>		{ ttk::notebook::Press %W %x %y }
bind TNotebook <Key-Right>		{ ttk::notebook::CycleTab %W  1; break }
bind TNotebook <Key-Left>		{ ttk::notebook::CycleTab %W -1; break }
bind TNotebook <Control-Key-Tab>	{ ttk::notebook::CycleTab %W  1; break }
bind TNotebook <Control-Shift-Key-Tab>	{ ttk::notebook::CycleTab %W -1; break }
catch {
bind TNotebook <Control-ISO_Left_Tab>	{ ttk::notebook::CycleTab %W -1; break }
}
bind TNotebook <Destroy>		{ ttk::notebook::Cleanup %W }

# ActivateTab $nb $tab --
#	Select the specified tab and set focus.
#
# 	If $tab was already the current tab, set the focus to the
#	notebook widget.  Otherwise, set the focus to the first
#	traversable widget in the pane.  The behavior is that the
#	notebook takes focus when the user selects the same tab
#	a second time.  This mirrors Windows tab behavior.
#
proc ttk::notebook::ActivateTab {w tab} {
    if {[$w index $tab] eq [$w index current]} {
	focus $w
    } else {
    	$w select $tab
	update ;# needed so focus logic sees correct mapped/unmapped states
	if {[set f [ttk::focusFirst [$w select]]] ne ""} {
	    tk::TabToWindow $f
	}
    }
}

# Press $nb $x $y --
#	ButtonPress-1 binding for notebook widgets.
#	Activate the tab under the mouse cursor, if any.
#
proc ttk::notebook::Press {w x y} {
    set index [$w index @$x,$y]
    if {$index ne ""} {
	ActivateTab $w $index
    }
}

# CycleTab --
#	Select the next/previous tab in the list.
#
proc ttk::notebook::CycleTab {w dir} {
    if {[$w index end] != 0} {
	set current [$w index current]
	set select [expr {($current + $dir) % [$w index end]}]
	while {[$w tab $select -state] != "normal" && ($select != $current)} {
	    set select [expr {($select + $dir) % [$w index end]}]
	}
	if {$select != $current} {
	    ActivateTab $w $select
	}
    }
}

# MnemonicTab $nb $key --
#	Scan all tabs in the specified notebook for one with the 
#	specified mnemonic. If found, returns path name of tab;
#	otherwise returns ""
#
proc ttk::notebook::MnemonicTab {nb key} {
    set key [string toupper $key]
    foreach tab [$nb tabs] {
	set label [$nb tab $tab -text]
	set underline [$nb tab $tab -underline]
	set mnemonic [string toupper [string index $label $underline]]
	if {$mnemonic ne "" && $mnemonic eq $key} {
	    return $tab
	}
    }
    return ""
}

# +++ Toplevel keyboard traversal.
#

# enableTraversal --
#	Enable keyboard traversal for a notebook widget
#	by adding bindings to the containing toplevel window.
#
#	TLNotebooks($top) keeps track of the list of all traversal-enabled 
#	notebooks contained in the toplevel 
#
proc ttk::notebook::enableTraversal {nb} {
    variable TLNotebooks

    set top [winfo toplevel $nb]

    if {![info exists TLNotebooks($top)]} {
	# Augment $top bindings:
	#
	bind $top <Control-Key-Tab> 	     {+ttk::notebook::TLCycleTab %W  1}
	bind $top <Shift-Control-Key-Tab>    {+ttk::notebook::TLCycleTab %W -1}
	catch {
	bind $top <Control-Key-ISO_Left_Tab> {+ttk::notebook::TLCycleTab %W -1}
	}
	if {[tk windowingsystem] eq "aqua"} {
	    bind $top <Option-KeyPress> \
		+[list ttk::notebook::MnemonicActivation $top %K]
	} else {
	    bind $top <Alt-KeyPress> \
		+[list ttk::notebook::MnemonicActivation $top %K]
	}
	bind $top <Destroy> {+ttk::notebook::TLCleanup %W}
    }

    lappend TLNotebooks($top) $nb
}

# TLCleanup -- <Destroy> binding for traversal-enabled toplevels
#
proc ttk::notebook::TLCleanup {w} {
    variable TLNotebooks
    if {$w eq [winfo toplevel $w]} {
	unset -nocomplain -please TLNotebooks($w)
    }
}

# Cleanup -- <Destroy> binding for notebooks
#
proc ttk::notebook::Cleanup {nb} {
    variable TLNotebooks
    set top [winfo toplevel $nb]
    if {[info exists TLNotebooks($top)]} {
	set index [lsearch -exact $TLNotebooks($top) $nb]
        set TLNotebooks($top) [lreplace $TLNotebooks($top) $index $index]
    }
}

# EnclosingNotebook $w -- 
#	Return the nearest traversal-enabled notebook widget
#	that contains $w.
#
# BUGS: this only works properly for tabs that are direct children
#	of the notebook widget.  This routine should follow the
#	geometry manager hierarchy, not window ancestry, but that
#	information is not available in Tk.
#
proc ttk::notebook::EnclosingNotebook {w} {
    variable TLNotebooks

    set top [winfo toplevel $w]
    if {![info exists TLNotebooks($top)]} { return }

    while {$w ne $top  && $w ne ""} {
	if {[lsearch -exact $TLNotebooks($top) $w] >= 0} {
	    return $w
	}
	set w [winfo parent $w]
    }
    return ""
}

# TLCycleTab --
#	toplevel binding procedure for Control-Tab / Shift-Control-Tab
#	Select the next/previous tab in the nearest ancestor notebook. 
#
proc ttk::notebook::TLCycleTab {w dir} {
    set nb [EnclosingNotebook $w]
    if {$nb ne ""} {
	CycleTab $nb $dir
	return -code break
    }
}

# MnemonicActivation $nb $key --
#	Alt-KeyPress binding procedure for mnemonic activation.
#	Scan all notebooks in specified toplevel for a tab with the
#	the specified mnemonic.  If found, activate it and return TCL_BREAK.
#
proc ttk::notebook::MnemonicActivation {top key} {
    variable TLNotebooks
    foreach nb $TLNotebooks($top) {
	if {[set tab [MnemonicTab $nb $key]] ne ""} {
	    ActivateTab $nb [$nb index $tab]
	    return -code break
	}
    }
}
