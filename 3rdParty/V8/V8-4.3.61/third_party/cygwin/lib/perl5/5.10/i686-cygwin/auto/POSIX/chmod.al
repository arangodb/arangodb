# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 577 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/chmod.al)"
sub chmod {
    usage "chmod(mode, filename)" if @_ != 2;
    CORE::chmod($_[0], $_[1]);
}

# end of POSIX::chmod
1;
