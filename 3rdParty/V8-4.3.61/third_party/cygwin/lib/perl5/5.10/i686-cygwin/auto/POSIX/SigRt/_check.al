# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 1018 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/_check.al)"
sub _check {
    my ($rtsig, $ok) = &_exist;
    die "No POSIX::SigRt signal $_[1] (valid range SIGRTMIN..SIGRTMAX, or $_SIGRTMIN..$_SIGRTMAX)"
	unless $ok;
    return $rtsig;
}

# end of POSIX::SigRt::_check
1;
