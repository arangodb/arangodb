# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigAction;

#line 986 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigAction/flags.al)"
sub flags   { $_[0]->{FLAGS}   = $_[1] if @_ > 1; $_[0]->{FLAGS} };
# end of POSIX::SigAction::flags
1;
