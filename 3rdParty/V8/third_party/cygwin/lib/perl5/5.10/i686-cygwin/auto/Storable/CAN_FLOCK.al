# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 83 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/CAN_FLOCK.al)"
#
# Determine whether locking is possible, but only when needed.
#

sub CAN_FLOCK; my $CAN_FLOCK; sub CAN_FLOCK {
	return $CAN_FLOCK if defined $CAN_FLOCK;
	require Config; import Config;
	return $CAN_FLOCK =
		$Config{'d_flock'} ||
		$Config{'d_fcntl_can_lock'} ||
		$Config{'d_lockf'};
}

# end of Storable::CAN_FLOCK
1;
