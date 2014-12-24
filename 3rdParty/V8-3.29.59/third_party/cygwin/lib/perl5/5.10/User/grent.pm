package User::grent;
use strict;

use 5.006_001;
our $VERSION = '1.01';
our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);
BEGIN { 
    use Exporter   ();
    @EXPORT      = qw(getgrent getgrgid getgrnam getgr);
    @EXPORT_OK   = qw($gr_name $gr_gid $gr_passwd $gr_mem @gr_members);
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars      @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'User::grent' => [
    name    => '$',
    passwd  => '$',
    gid	    => '$',
    members => '@',
];

sub populate (@) {
    return unless @_;
    my $gob = new();
    ($gr_name, $gr_passwd, $gr_gid) = @$gob[0,1,2] = @_[0,1,2];
    @gr_members = @{$gob->[3]} = split ' ', $_[3];
    return $gob;
} 

sub getgrent ( ) { populate(CORE::getgrent()) } 
sub getgrnam ($) { populate(CORE::getgrnam(shift)) } 
sub getgrgid ($) { populate(CORE::getgrgid(shift)) } 
sub getgr    ($) { ($_[0] =~ /^\d+/) ? &getgrgid : &getgrnam } 

1;
__END__

=head1 NAME

User::grent - by-name interface to Perl's built-in getgr*() functions

=head1 SYNOPSIS

 use User::grent;
 $gr = getgrgid(0) or die "No group zero";
 if ( $gr->name eq 'wheel' && @{$gr->members} > 1 ) {
     print "gid zero name wheel, with other members";
 } 

 use User::grent qw(:FIELDS);
 getgrgid(0) or die "No group zero";
 if ( $gr_name eq 'wheel' && @gr_members > 1 ) {
     print "gid zero name wheel, with other members";
 } 

 $gr = getgr($whoever);

=head1 DESCRIPTION

This module's default exports override the core getgrent(), getgruid(),
and getgrnam() functions, replacing them with versions that return
"User::grent" objects.  This object has methods that return the similarly
named structure field name from the C's passwd structure from F<grp.h>; 
namely name, passwd, gid, and members (not mem).  The first three
return scalars, the last an array reference.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as variables named
with a preceding C<gr_>.  Thus, C<$group_obj-E<gt>gid()> corresponds
to $gr_gid if you import the fields.  Array references are available as
regular array variables, so C<@{ $group_obj-E<gt>members() }> would be
simply @gr_members.

The getpw() function is a simple front-end that forwards
a numeric argument to getpwuid() and the rest to getpwnam().

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
