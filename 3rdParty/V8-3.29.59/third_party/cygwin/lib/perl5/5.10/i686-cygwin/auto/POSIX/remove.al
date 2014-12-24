# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 366 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/remove.al)"
sub remove {
    usage "remove(filename)" if @_ != 1;
    (-d $_[0]) ? CORE::rmdir($_[0]) : CORE::unlink($_[0]);
}

# end of POSIX::remove
1;
