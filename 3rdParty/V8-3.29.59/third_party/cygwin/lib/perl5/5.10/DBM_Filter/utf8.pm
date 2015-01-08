package DBM_Filter::utf8 ;

use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

BEGIN
{
    eval { require Encode; };

    croak "Encode module not found.\n"
        if $@;
}

sub Store { $_ = Encode::encode_utf8($_) if defined $_ }

sub Fetch { $_ = Encode::decode_utf8($_) if defined $_ }

1;

__END__

=head1 NAME

DBM_Filter::utf8 - filter for DBM_Filter

=head1 SYNOPSIS

    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File
    use DBM_Filter ;

    $db = tie %hash, ...
    $db->Filter_Push('utf8');

=head1 DESCRIPTION

This Filter will ensure that all data written to the DBM will be encoded
in UTF-8.

This module uses the Encode module.

=head1 SEE ALSO

L<DBM_Filter>, L<perldbmfilter>, L<Encode>

=head1 AUTHOR

Paul Marquess pmqs@cpan.org

