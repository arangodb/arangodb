# NOTE: Derived from ../../lib/Storable.pm.
# Changes made here will be lost when autosplit is run again.
# See AutoSplit.pm.
package Storable;

#line 196 "../../lib/Storable.pm (autosplit into ../../lib/auto/Storable/BIN_WRITE_VERSION_NV.al)"
sub BIN_WRITE_VERSION_NV {
    sprintf "%d.%03d", BIN_MAJOR(), BIN_WRITE_MINOR();
}

# end of Storable::BIN_WRITE_VERSION_NV
1;
