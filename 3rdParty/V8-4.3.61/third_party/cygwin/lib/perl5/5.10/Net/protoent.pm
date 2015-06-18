package Net::protoent;
use strict;

use 5.006_001;
our $VERSION = '1.00';
our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);
BEGIN { 
    use Exporter   ();
    @EXPORT      = qw(getprotobyname getprotobynumber getprotoent getproto);
    @EXPORT_OK   = qw( $p_name @p_aliases $p_proto );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars      @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'Net::protoent' => [
   name		=> '$',
   aliases	=> '@',
   proto	=> '$',
];

sub populate (@) {
    return unless @_;
    my $pob = new();
    $p_name 	 =    $pob->[0]     	     = $_[0];
    @p_aliases	 = @{ $pob->[1] } = split ' ', $_[1];
    $p_proto	 =    $pob->[2] 	     = $_[2];
    return $pob;
} 

sub getprotoent      ( )  { populate(CORE::getprotoent()) } 
sub getprotobyname   ($)  { populate(CORE::getprotobyname(shift)) } 
sub getprotobynumber ($)  { populate(CORE::getprotobynumber(shift)) } 

sub getproto ($;$) {
    no strict 'refs';
    return &{'getprotoby' . ($_[0]=~/^\d+$/ ? 'number' : 'name')}(@_);
}

1;

__END__

=head1 NAME

Net::protoent - by-name interface to Perl's built-in getproto*() functions

=head1 SYNOPSIS

 use Net::protoent;
 $p = getprotobyname(shift || 'tcp') || die "no proto";
 printf "proto for %s is %d, aliases are %s\n",
    $p->name, $p->proto, "@{$p->aliases}";

 use Net::protoent qw(:FIELDS);
 getprotobyname(shift || 'tcp') || die "no proto";
 print "proto for $p_name is $p_proto, aliases are @p_aliases\n";

=head1 DESCRIPTION

This module's default exports override the core getprotoent(),
getprotobyname(), and getnetbyport() functions, replacing them with
versions that return "Net::protoent" objects.  They take default
second arguments of "tcp".  This object has methods that return the
similarly named structure field name from the C's protoent structure
from F<netdb.h>; namely name, aliases, and proto.  The aliases method
returns an array reference, the rest scalars.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as variables named
with a preceding C<p_>.  Thus, C<$proto_obj-E<gt>name()> corresponds to
$p_name if you import the fields.  Array references are available as
regular array variables, so for example C<@{ $proto_obj-E<gt>aliases()
}> would be simply @p_aliases.

The getproto() function is a simple front-end that forwards a numeric
argument to getprotobyport(), and the rest to getprotobyname().

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
