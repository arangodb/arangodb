if {[catch {package present Tcl 8.5.0-8.6}]} { return }
package ifneeded Tk 8.5.7 [list load [file join $dir .. .. bin tk85.dll] Tk]
