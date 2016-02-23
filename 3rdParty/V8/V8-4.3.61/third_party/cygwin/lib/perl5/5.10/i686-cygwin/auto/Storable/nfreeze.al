# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 321 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/nfreeze.al)"
#
# nfreeze
#
# Same as freeze but in network order.
#
sub nfreeze {
	_freeze(\&net_mstore, @_);
}

# end of Storable::nfreeze
1;
