package DBM_Filter::compress ;

use strict;
use warnings;
use Carp;

our $VERSION = '0.01';

BEGIN
{
    eval { require Compress::Zlib; Compress::Zlib->import() };

    croak "Compress::Zlib module not found.\n"
        if $@;
}



sub Store { $_ = compress($_) }
sub Fetch { $_ = uncompress($_) }

1;

__END__

=head1 NAME

DBM_Filter::compress - filter for DBM_Filter

=head1 SYNOPSIS

    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File
    use DBM_Filter ;

    $db = tie %hash, ...
    $db->Filter_Push('compress');

=head1 DESCRIPTION

This DBM filter will compress all data before it is written to the database
and uncompressed it on reading.

A fatal error will be thrown if the Compress::Zlib module is not
available.

=head1 SEE ALSO

L<DBM_Filter>, L<perldbmfilter>, L<Compress::Zlib>

=head1 AUTHOR

Paul Marquess pmqs@cpan.org

