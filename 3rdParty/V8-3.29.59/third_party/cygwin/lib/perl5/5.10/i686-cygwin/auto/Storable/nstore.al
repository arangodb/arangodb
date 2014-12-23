# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 212 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/nstore.al)"
#
# nstore
#
# Same as store, but in network order.
#
sub nstore {
	return _store(\&net_pstore, @_, 0);
}

# end of Storable::nstore
1;
