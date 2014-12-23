# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 1012 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/_exist.al)"
sub _exist {
    my $rtsig = _getsig($_[1]);
    my $ok    = $rtsig >= $_SIGRTMIN && $rtsig <= $_SIGRTMAX;
    ($rtsig, $ok);
}

# end of POSIX::SigRt::_exist
1;
