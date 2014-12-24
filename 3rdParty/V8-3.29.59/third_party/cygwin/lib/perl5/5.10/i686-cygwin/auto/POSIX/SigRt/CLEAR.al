# NOTE: Derived from ../../lib/POSIX.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package POSIX::SigRt;

#line 1041 "../../lib/POSIX.pm (autosplit into ../../lib/auto/POSIX/SigRt/CLEAR.al)"
sub CLEAR  { &_exist; delete @SIG{ &POSIX::SIGRTMIN .. &POSIX::SIGRTMAX } }
# end of POSIX::SigRt::CLEAR
1;
