package DBM_Filter::int32 ;

use strict;
use warnings;

our $VERSION = '0.01';

# todo get Filter to figure endian.

sub Store
{
    $_ = 0 if ! defined $_ || $_ eq "" ;
    $_ = pack("i", $_);
}

sub Fetch
{
    no warnings 'uninitialized';
    $_ = unpack("i", $_);
}

1;

__END__

=head1 NAME

DBM_Filter::int32 - filter for DBM_Filter

=head1 SYNOPSIS

    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File
    use DBM_Filter ;

    $db = tie %hash, ...
    $db->Filter_Push('int32');

=head1 DESCRIPTION

This DBM filter is used when interoperating with a C/C++ application
that uses a C int as either the key and/or value in the DBM file.

=head1 SEE ALSO

L<DBM_Filter>, L<perldbmfilter>

=head1 AUTHOR

Paul Marquess pmqs@cpan.org

