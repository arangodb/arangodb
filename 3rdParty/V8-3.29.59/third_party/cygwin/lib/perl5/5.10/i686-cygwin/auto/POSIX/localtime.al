# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 621 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/localtime.al)"
sub localtime {
    usage "localtime(time)" if @_ != 1;
    CORE::localtime($_[0]);
}

# end of POSIX::localtime
1;
