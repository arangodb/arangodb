package File::stat;
use 5.006;

use strict;
use warnings;

our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);

our $VERSION = '1.00';

BEGIN { 
    use Exporter   ();
    @EXPORT      = qw(stat lstat);
    @EXPORT_OK   = qw( $st_dev	   $st_ino    $st_mode 
		       $st_nlink   $st_uid    $st_gid 
		       $st_rdev    $st_size 
		       $st_atime   $st_mtime  $st_ctime 
		       $st_blksize $st_blocks
		    );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'File::stat' => [
     map { $_ => '$' } qw{
	 dev ino mode nlink uid gid rdev size
	 atime mtime ctime blksize blocks
     }
];

sub populate (@) {
    return unless @_;
    my $stob = new();
    @$stob = (
	$st_dev, $st_ino, $st_mode, $st_nlink, $st_uid, $st_gid, $st_rdev,
        $st_size, $st_atime, $st_mtime, $st_ctime, $st_blksize, $st_blocks ) 
	    = @_;
    return $stob;
} 

sub lstat ($)  { populate(CORE::lstat(shift)) }

sub stat ($) {
    my $arg = shift;
    my $st = populate(CORE::stat $arg);
    return $st if $st;
	my $fh;
    {
		local $!;
		no strict 'refs';
		require Symbol;
		$fh = \*{ Symbol::qualify( $arg, caller() )};
		return unless defined fileno $fh;
	}
    return populate(CORE::stat $fh);
}

1;
__END__

=head1 NAME

File::stat - by-name interface to Perl's built-in stat() functions

=head1 SYNOPSIS

 use File::stat;
 $st = stat($file) or die "No $file: $!";
 if ( ($st->mode & 0111) && $st->nlink > 1) ) {
     print "$file is executable with lotsa links\n";
 } 

 use File::stat qw(:FIELDS);
 stat($file) or die "No $file: $!";
 if ( ($st_mode & 0111) && $st_nlink > 1) ) {
     print "$file is executable with lotsa links\n";
 } 

=head1 DESCRIPTION

This module's default exports override the core stat() 
and lstat() functions, replacing them with versions that return 
"File::stat" objects.  This object has methods that
return the similarly named structure field name from the
stat(2) function; namely,
dev,
ino,
mode,
nlink,
uid,
gid,
rdev,
size,
atime,
mtime,
ctime,
blksize,
and
blocks.  

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your stat() and lstat() functions.)  Access these fields as
variables named with a preceding C<st_> in front their method names.
Thus, C<$stat_obj-E<gt>dev()> corresponds to $st_dev if you import
the fields.

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 BUGS

As of Perl 5.8.0 after using this module you cannot use the implicit
C<$_> or the special filehandle C<_> with stat() or lstat(), trying
to do so leads into strange errors.  The workaround is for C<$_> to
be explicit

    my $stat_obj = stat $_;

and for C<_> to explicitly populate the object using the unexported
and undocumented populate() function with CORE::stat():

    my $stat_obj = File::stat::populate(CORE::stat(_));

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
