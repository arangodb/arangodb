# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 706 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/getpid.al)"
sub getpid {
    usage "getpid()" if @_ != 0;
    $$;
}

# end of POSIX::getpid
1;
