# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX;

#line 582 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/fstat.al)"
sub fstat {
    usage "fstat(fd)" if @_ != 1;
    local *TMP;
    CORE::open(TMP, "<&$_[0]");		# Gross.
    my @l = CORE::stat(TMP);
    CORE::close(TMP);
    @l;
}

# end of POSIX::fstat
1;
