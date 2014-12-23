# history.tcl --
#
# Implementation of the history command.
#
# RCS: @(#) $Id: history.tcl,v 1.7 2005/07/23 04:12:49 dgp Exp $
#
# Copyright (c) 1997 Sun Microsystems, Inc.
#
# See the file "license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.
#

# The tcl::history array holds the history list and
# some additional bookkeeping variables.
#
# nextid	the index used for the next history list item.
# keep		the max size of the history list
# oldest	the index of the oldest item in the history.

namespace eval tcl {
    variable history
    if {![info exists history]} {
	array set history {
	    nextid	0
	    keep	20
	    oldest	-20
	}
    }
}

# history --
#
#	This is the main history command.  See the man page for its interface.
#	This does argument checking and calls helper procedures in the
#	history namespace.

proc history {args} {
    set len [llength $args]
    if {$len == 0} {
	return [tcl::HistInfo]
    }
    set key [lindex $args 0]
    set options "add, change, clear, event, info, keep, nextid, or redo"
    switch -glob -- $key {
	a* { # history add

	    if {$len > 3} {
		return -code error "wrong # args: should be \"history add event ?exec?\""
	    }
	    if {![string match $key* add]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    if {$len == 3} {
		set arg [lindex $args 2]
		if {! ([string match e* $arg] && [string match $arg* exec])} {
		    return -code error "bad argument \"$arg\": should be \"exec\""
		}
	    }
	    return [tcl::HistAdd [lindex $args 1] [lindex $args 2]]
	}
	ch* { # history change

	    if {($len > 3) || ($len < 2)} {
		return -code error "wrong # args: should be \"history change newValue ?event?\""
	    }
	    if {![string match $key* change]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    if {$len == 2} {
		set event 0
	    } else {
		set event [lindex $args 2]
	    }

	    return [tcl::HistChange [lindex $args 1] $event]
	}
	cl* { # history clear

	    if {($len > 1)} {
		return -code error "wrong # args: should be \"history clear\""
	    }
	    if {![string match $key* clear]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    return [tcl::HistClear]
	}
	e* { # history event

	    if {$len > 2} {
		return -code error "wrong # args: should be \"history event ?event?\""
	    }
	    if {![string match $key* event]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    if {$len == 1} {
		set event -1
	    } else {
		set event [lindex $args 1]
	    }
	    return [tcl::HistEvent $event]
	}
	i* { # history info

	    if {$len > 2} {
		return -code error "wrong # args: should be \"history info ?count?\""
	    }
	    if {![string match $key* info]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    return [tcl::HistInfo [lindex $args 1]]
	}
	k* { # history keep

	    if {$len > 2} {
		return -code error "wrong # args: should be \"history keep ?count?\""
	    }
	    if {$len == 1} {
		return [tcl::HistKeep]
	    } else {
		set limit [lindex $args 1]
		if {[catch {expr {~$limit}}] || ($limit < 0)} {
		    return -code error "illegal keep count \"$limit\""
		}
		return [tcl::HistKeep $limit]
	    }
	}
	n* { # history nextid

	    if {$len > 1} {
		return -code error "wrong # args: should be \"history nextid\""
	    }
	    if {![string match $key* nextid]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    return [expr {$tcl::history(nextid) + 1}]
	}
	r* { # history redo

	    if {$len > 2} {
		return -code error "wrong # args: should be \"history redo ?event?\""
	    }
	    if {![string match $key* redo]} {
		return -code error "bad option \"$key\": must be $options"
	    }
	    return [tcl::HistRedo [lindex $args 1]]
	}
	default {
	    return -code error "bad option \"$key\": must be $options"
	}
    }
}

# tcl::HistAdd --
#
#	Add an item to the history, and optionally eval it at the global scope
#
# Parameters:
#	command		the command to add
#	exec		(optional) a substring of "exec" causes the
#			command to be evaled.
# Results:
# 	If executing, then the results of the command are returned
#
# Side Effects:
#	Adds to the history list

 proc tcl::HistAdd {command {exec {}}} {
    variable history

    # Do not add empty commands to the history
    if {[string trim $command] eq ""} {
	return ""
    }

    set i [incr history(nextid)]
    set history($i) $command
    set j [incr history(oldest)]
    unset -nocomplain history($j)
    if {[string match e* $exec]} {
	return [uplevel #0 $command]
    } else {
	return {}
    }
}

# tcl::HistKeep --
#
#	Set or query the limit on the length of the history list
#
# Parameters:
#	limit	(optional) the length of the history list
#
# Results:
#	If no limit is specified, the current limit is returned
#
# Side Effects:
#	Updates history(keep) if a limit is specified

 proc tcl::HistKeep {{limit {}}} {
    variable history
    if {$limit eq ""} {
	return $history(keep)
    } else {
	set oldold $history(oldest)
	set history(oldest) [expr {$history(nextid) - $limit}]
	for {} {$oldold <= $history(oldest)} {incr oldold} {
	    unset -nocomplain history($oldold)
	}
	set history(keep) $limit
    }
}

# tcl::HistClear --
#
#	Erase the history list
#
# Parameters:
#	none
#
# Results:
#	none
#
# Side Effects:
#	Resets the history array, except for the keep limit

 proc tcl::HistClear {} {
    variable history
    set keep $history(keep)
    unset history
    array set history [list \
	nextid	0	\
	keep	$keep	\
	oldest	-$keep	\
    ]
}

# tcl::HistInfo --
#
#	Return a pretty-printed version of the history list
#
# Parameters:
#	num	(optional) the length of the history list to return
#
# Results:
#	A formatted history list

 proc tcl::HistInfo {{num {}}} {
    variable history
    if {$num eq ""} {
	set num [expr {$history(keep) + 1}]
    }
    set result {}
    set newline ""
    for {set i [expr {$history(nextid) - $num + 1}]} \
	    {$i <= $history(nextid)} {incr i} {
	if {![info exists history($i)]} {
	    continue
	}
        set cmd [string map [list \n \n\t] [string trimright $history($i) \ \n]]
	append result $newline[format "%6d  %s" $i $cmd]
	set newline \n
    }
    return $result
}

# tcl::HistRedo --
#
#	Fetch the previous or specified event, execute it, and then
#	replace the current history item with that event.
#
# Parameters:
#	event	(optional) index of history item to redo.  Defaults to -1,
#		which means the previous event.
#
# Results:
#	Those of the command being redone.
#
# Side Effects:
#	Replaces the current history list item with the one being redone.

 proc tcl::HistRedo {{event -1}} {
    variable history
    if {$event eq ""} {
	set event -1
    }
    set i [HistIndex $event]
    if {$i == $history(nextid)} {
	return -code error "cannot redo the current event"
    }
    set cmd $history($i)
    HistChange $cmd 0
    uplevel #0 $cmd
}

# tcl::HistIndex --
#
#	Map from an event specifier to an index in the history list.
#
# Parameters:
#	event	index of history item to redo.
#		If this is a positive number, it is used directly.
#		If it is a negative number, then it counts back to a previous
#		event, where -1 is the most recent event.
#		A string can be matched, either by being the prefix of
#		a command or by matching a command with string match.
#
# Results:
#	The index into history, or an error if the index didn't match.

 proc tcl::HistIndex {event} {
    variable history
    if {[catch {expr {~$event}}]} {
	for {set i [expr {$history(nextid)-1}]} {[info exists history($i)]} \
		{incr i -1} {
	    if {[string match $event* $history($i)]} {
		return $i;
	    }
	    if {[string match $event $history($i)]} {
		return $i;
	    }
	}
	return -code error "no event matches \"$event\""
    } elseif {$event <= 0} {
	set i [expr {$history(nextid) + $event}]
    } else {
	set i $event
    }
    if {$i <= $history(oldest)} {
	return -code error "event \"$event\" is too far in the past"
    }
    if {$i > $history(nextid)} {
	return -code error "event \"$event\" hasn't occured yet"
    }
    return $i
}

# tcl::HistEvent --
#
#	Map from an event specifier to the value in the history list.
#
# Parameters:
#	event	index of history item to redo.  See index for a
#		description of possible event patterns.
#
# Results:
#	The value from the history list.

 proc tcl::HistEvent {event} {
    variable history
    set i [HistIndex $event]
    if {[info exists history($i)]} {
	return [string trimright $history($i) \ \n]
    } else {
	return "";
    }
}

# tcl::HistChange --
#
#	Replace a value in the history list.
#
# Parameters:
#	cmd	The new value to put into the history list.
#	event	(optional) index of history item to redo.  See index for a
#		description of possible event patterns.  This defaults
#		to 0, which specifies the current event.
#
# Side Effects:
#	Changes the history list.

 proc tcl::HistChange {cmd {event 0}} {
    variable history
    set i [HistIndex $event]
    set history($i) $cmd
}
