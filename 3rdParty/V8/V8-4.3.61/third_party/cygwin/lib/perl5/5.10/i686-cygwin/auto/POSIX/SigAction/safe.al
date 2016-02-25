# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigAction;

#line 987 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigAction/safe.al)"
sub safe    { $_[0]->{SAFE}    = $_[1] if @_ > 1; $_[0]->{SAFE} };

package POSIX::SigRt;


# end of POSIX::SigRt::safe
1;
