;# timelocal.pl
;#
;# Usage:
;#	$time = timelocal($sec,$min,$hours,$mday,$mon,$year);
;#	$time = timegm($sec,$min,$hours,$mday,$mon,$year);

;# This file has been superseded by the Time::Local library module.
;# It is implemented as a call to that module for backwards compatibility
;# with code written for perl4; new code should use Time::Local directly.

;# The current implementation shares with the original the questionable
;# behavior of defining the timelocal() and timegm() functions in the
;# namespace of whatever package was current when the first instance of
;# C<require 'timelocal.pl';> was executed in a program.

use Time::Local;

*timelocal::cheat = \&Time::Local::cheat;
