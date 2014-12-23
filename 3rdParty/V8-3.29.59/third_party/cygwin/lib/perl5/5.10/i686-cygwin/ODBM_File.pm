package ODBM_File;

use strict;
use warnings;

require Tie::Hash;
use XSLoader ();

our @ISA = qw(Tie::Hash);
our $VERSION = "1.07";

XSLoader::load 'ODBM_File', $VERSION;

1;

__END__

=head1 NAME

ODBM_File - Tied access to odbm files

=head1 SYNOPSIS

 use Fcntl;   # For O_RDWR, O_CREAT, etc.
 use ODBM_File;

  # Now read and change the hash
  $h{newkey} = newvalue;
  print $h{oldkey}; 
  ...

  untie %h;

=head1 DESCRIPTION

C<ODBM_File> establishes a connection between a Perl hash variable and
a file in ODBM_File format;.  You can manipulate the data in the file
just as if it were in a Perl hash, but when your program exits, the
data will remain in the file, to be used the next time your program
runs.

Use C<ODBM_File> with the Perl built-in C<tie> function to establish
the connection between the variable and the file.  The arguments to
C<tie> should be:

=over 4

=item 1.

The hash variable you want to tie.

=item 2. 

The string C<"ODBM_File">.  (Ths tells Perl to use the C<ODBM_File>
package to perform the functions of the hash.)

=item 3. 

The name of the file you want to tie to the hash.  

=item 4.

Flags.  Use one of:

=over 2

=item C<O_RDONLY>

Read-only access to the data in the file.

=item C<O_WRONLY>

Write-only access to the data in the file.

=item C<O_RDWR>

Both read and write access.

=back

If you want to create the file if it does not exist, add C<O_CREAT> to
any of these, as in the example.  If you omit C<O_CREAT> and the file
does not already exist, the C<tie> call will fail.

=item 5.

The default permissions to use if a new file is created.  The actual
permissions will be modified by the user's umask, so you should
probably use 0666 here. (See L<perlfunc/umask>.)

=back

=head1 DIAGNOSTICS

On failure, the C<tie> call returns an undefined value and probably
sets C<$!> to contain the reason the file could not be tied.

=head2 C<odbm store returned -1, errno 22, key "..." at ...>

This warning is emitted when you try to store a key or a value that
is too long.  It means that the change was not recorded in the
database.  See BUGS AND WARNINGS below.

=head1 BUGS AND WARNINGS

There are a number of limits on the size of the data that you can
store in the ODBM file.  The most important is that the length of a
key, plus the length of its associated value, may not exceed 1008
bytes.

See L<perlfunc/tie>, L<perldbmfilter>, L<Fcntl>

=cut
