#!/bin/sh
# the next line restarts using wish \
exec wish "$0" "$@"

# hello --
# Simple Tk script to create a button that prints "Hello, world".
# Click on the button to terminate the program.
#
# RCS: @(#) $Id: hello,v 1.4 2003/09/30 14:54:30 dkf Exp $

package require Tk

# The first line below creates the button, and the second line
# asks the packer to shrink-wrap the application's main window
# around the button.

button .hello -text "Hello, world" -command {
    puts stdout "Hello, world"; destroy .
}
pack .hello

# Local Variables:
# mode: tcl
# End:
