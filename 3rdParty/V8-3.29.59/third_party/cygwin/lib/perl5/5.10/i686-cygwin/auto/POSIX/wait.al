# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 606 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/wait.al)"
sub wait {
    usage "wait()" if @_ != 0;
    CORE::wait();
}

# end of POSIX::wait
1;
