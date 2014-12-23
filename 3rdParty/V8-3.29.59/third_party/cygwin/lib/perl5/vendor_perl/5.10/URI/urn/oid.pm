package URI::urn::oid;  # RFC 2061

require URI::urn;
@ISA=qw(URI::urn);

use strict;

sub oid {
    my $self = shift;
    my $old = $self->nss;
    if (@_) {
	$self->nss(join(".", @_));
    }
    return split(/\./, $old) if wantarray;
    return $old;
}

1;
