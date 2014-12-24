# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigAction;

#line 981 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigAction/new.al)"
package POSIX::SigAction;

sub new { bless {HANDLER => $_[1], MASK => $_[2], FLAGS => $_[3] || 0, SAFE => 0}, $_[0] }
# end of POSIX::SigAction::new
1;
