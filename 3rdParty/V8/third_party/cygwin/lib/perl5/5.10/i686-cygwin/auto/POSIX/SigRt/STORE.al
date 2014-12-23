# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 1039 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/STORE.al)"
sub STORE  { my $rtsig = &_check; new($rtsig, $_[2], $SIGACTION_FLAGS) }
# end of POSIX::SigRt::STORE
1;
