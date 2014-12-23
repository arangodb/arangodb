#
# $Id: aquaTheme.tcl,v 1.11.2.1 2008/07/22 17:02:31 das Exp $
#
# Aqua theme (OSX native look and feel)
#

namespace eval ttk::theme::aqua {
    ttk::style theme settings aqua {

	ttk::style configure . \
	    -font TkDefaultFont \
	    -background systemWindowBody \
	    -foreground systemModelessDialogActiveText \
	    -selectbackground systemHighlight \
	    -selectforeground systemModelessDialogActiveText \
	    -selectborderwidth 0 \
	    -insertwidth 1

	ttk::style map . \
	    -foreground {disabled systemModelessDialogInactiveText
		    background systemModelessDialogInactiveText} \
	    -selectbackground {background systemHighlightSecondary
		    !focus systemHighlightSecondary} \
	    -selectforeground {background systemModelessDialogInactiveText
		    !focus systemDialogActiveText}

	# Workaround for #1100117:
	# Actually, on Aqua we probably shouldn't stipple images in
	# disabled buttons even if it did work...
	ttk::style configure . -stipple {}

	ttk::style configure TButton -anchor center -width -6
	ttk::style configure Toolbutton -padding 4
	# See Apple HIG figs 14-63, 14-65
	ttk::style configure TNotebook -tabposition n -padding {20 12}
	ttk::style configure TNotebook.Tab -padding {10 2 10 2}

	# Combobox:
	ttk::style configure TCombobox -postoffset {5 -2 -10 0}

	# Treeview:
	ttk::style configure Treeview -rowheight 18
	ttk::style configure Heading -font TkHeadingFont
	ttk::style map Row \
	    -background {{selected background} systemHighlightSecondary
		    selected systemHighlight}
	ttk::style map Cell \
	    -foreground {{selected background} systemModelessDialogInactiveText
		    selected systemModelessDialogActiveText}
	ttk::style map Item \
	    -foreground {{selected background} systemModelessDialogInactiveText
		    selected systemModelessDialogActiveText}

	# Enable animation for ttk::progressbar widget:
	ttk::style configure TProgressbar -period 100 -maxphase 255

	# For Aqua, labelframe labels should appear outside the border,
	# with a 14 pixel inset and 4 pixels spacing between border and label
	# (ref: Apple Human Interface Guidelines / Controls / Grouping Controls)
	#
	ttk::style configure TLabelframe \
		-labeloutside true -labelmargins {14 0 14 4}

	# TODO: panedwindow sashes should be 9 pixels (HIG:Controls:Split Views)
    }
}
