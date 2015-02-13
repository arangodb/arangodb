if {[catch {package require Tcl 8.4}]} return
package ifneeded Tix 8.4.3  [list load [file join $dir Tix843.dll] Tix]
