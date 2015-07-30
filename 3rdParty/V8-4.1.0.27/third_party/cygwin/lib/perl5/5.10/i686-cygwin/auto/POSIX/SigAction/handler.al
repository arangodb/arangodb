# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigAction;

#line 984 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigAction/handler.al)"
sub handler { $_[0]->{HANDLER} = $_[1] if @_ > 1; $_[0]->{HANDLER} };
# end of POSIX::SigAction::handler
1;
