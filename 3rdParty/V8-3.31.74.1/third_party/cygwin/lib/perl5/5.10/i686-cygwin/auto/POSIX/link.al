# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 726 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/link.al)"
sub link {
    usage "link(oldfilename, newfilename)" if @_ != 2;
    CORE::link($_[0], $_[1]);
}

# end of POSIX::link
1;
