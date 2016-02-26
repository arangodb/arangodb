# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 992 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/_init.al)"
sub _init {
    $_SIGRTMIN = &POSIX::SIGRTMIN;
    $_SIGRTMAX = &POSIX::SIGRTMAX;
    $_sigrtn   = $_SIGRTMAX - $_SIGRTMIN;
}

# end of POSIX::SigRt::_init
1;
