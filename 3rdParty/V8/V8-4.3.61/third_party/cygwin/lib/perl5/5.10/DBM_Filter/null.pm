package DBM_Filter::null ;

use strict;
use warnings;

our $VERSION = '0.01';

sub Store
{
    no warnings 'uninitialized';
    $_ .= "\x00" ;
}

sub Fetch
{
    no warnings 'uninitialized';
    s/\x00$// ;
}

1;

__END__

=head1 NAME

DBM_Filter::null - filter for DBM_Filter

=head1 SYNOPSIS

    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File
    use DBM_Filter ;

    $db = tie %hash, ...
    $db->Filter_Push('null');

=head1 DESCRIPTION

This filter ensures that all data written to the DBM file is null
terminated. This is useful when you have a perl script that needs
to interoperate with a DBM file that a C program also uses. A fairly
common issue is for the C application to include the terminating null
in a string when it writes to the DBM file. This filter will ensure that
all data written to the DBM file can be read by the C application.


=head1 SEE ALSO

L<DBM_Filter>, L<perldbmfilter>

=head1 AUTHOR

Paul Marquess pmqs@cpan.org
