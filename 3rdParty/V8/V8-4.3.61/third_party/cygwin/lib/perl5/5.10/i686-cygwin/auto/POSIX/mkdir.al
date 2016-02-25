# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 591 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/mkdir.al)"
sub mkdir {
    usage "mkdir(directoryname, mode)" if @_ != 2;
    CORE::mkdir($_[0], $_[1]);
}

# end of POSIX::mkdir
1;
