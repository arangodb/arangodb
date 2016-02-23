# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 998 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/_croak.al)"
sub _croak {
    &_init unless defined $_sigrtn;
    die "POSIX::SigRt not available" unless defined $_sigrtn && $_sigrtn > 0;
}

# end of POSIX::SigRt::_croak
1;
