# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigAction;

#line 985 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigAction/mask.al)"
sub mask    { $_[0]->{MASK}    = $_[1] if @_ > 1; $_[0]->{MASK} };
# end of POSIX::SigAction::mask
1;
