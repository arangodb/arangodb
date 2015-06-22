package URI::urn::isbn;  # RFC 3187

require URI::urn;
@ISA=qw(URI::urn);

use strict;
use Carp qw(carp);

BEGIN {
    require Business::ISBN;
    
    local $^W = 0; # don't warn about dev versions, perl5.004 style
    warn "Using Business::ISBN version " . Business::ISBN->VERSION . 
        " which is deprecated.\nUpgrade to Business::ISBN version 2\n"
        if Business::ISBN->VERSION < 2;
    }
    
sub _isbn {
    my $nss = shift;
    $nss = $nss->nss if ref($nss);
    my $isbn = Business::ISBN->new($nss);
    $isbn = undef if $isbn && !$isbn->is_valid;
    return $isbn;
}

sub _nss_isbn {
    my $self = shift;
    my $nss = $self->nss(@_);
    my $isbn = _isbn($nss);
    $isbn = $isbn->as_string if $isbn;
    return($nss, $isbn);
}

sub isbn {
    my $self = shift;
    my $isbn;
    (undef, $isbn) = $self->_nss_isbn(@_);
    return $isbn;
}

sub isbn_publisher_code {
    my $isbn = shift->_isbn || return undef;
    return $isbn->publisher_code;
}

BEGIN {
my $group_method = do {
    local $^W = 0; # don't warn about dev versions, perl5.004 style
    Business::ISBN->VERSION >= 2 ? 'group_code' : 'country_code';
    };

sub isbn_group_code {
    my $isbn = shift->_isbn || return undef;
    return $isbn->$group_method;
}
}

sub isbn_country_code {
    my $name = (caller(0))[3]; $name =~ s/.*:://;
    carp "$name is DEPRECATED. Use isbn_group_code instead";
    
    no strict 'refs';
    &isbn_group_code;
}

BEGIN {
my $isbn13_method = do {
    local $^W = 0; # don't warn about dev versions, perl5.004 style
    Business::ISBN->VERSION >= 2 ? 'as_isbn13' : 'as_ean';
    };

sub isbn13 {
    my $isbn = shift->_isbn || return undef;
    
    # Business::ISBN 1.x didn't put hyphens in the EAN, and it was just a string
    # Business::ISBN 2.0 doesn't do EAN, but it does ISBN-13 objects
    #   and it uses the hyphens, so call as_string with an empty anon array
    # or, adjust the test and features to say that it comes out with hyphens.
    my $thingy = $isbn->$isbn13_method;
    return eval { $thingy->can( 'as_string' ) } ? $thingy->as_string([]) : $thingy;
}
}

sub isbn_as_ean {
    my $name = (caller(0))[3]; $name =~ s/.*:://;
    carp "$name is DEPRECATED. Use isbn13 instead";

    no strict 'refs';
    &isbn13;
}

sub canonical {
    my $self = shift;
    my($nss, $isbn) = $self->_nss_isbn;
    my $new = $self->SUPER::canonical;
    return $new unless $nss && $isbn && $nss ne $isbn;
    $new = $new->clone if $new == $self;
    $new->nss($isbn);
    return $new;
}

1;
