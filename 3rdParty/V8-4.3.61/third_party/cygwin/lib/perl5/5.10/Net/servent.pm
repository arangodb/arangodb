package Net::servent;
use strict;

use 5.006_001;
our $VERSION = '1.01';
our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);
BEGIN {
    use Exporter   ();
    @EXPORT      = qw(getservbyname getservbyport getservent getserv);
    @EXPORT_OK   = qw( $s_name @s_aliases $s_port $s_proto );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars      @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'Net::servent' => [
   name		=> '$',
   aliases	=> '@',
   port		=> '$',
   proto	=> '$',
];

sub populate (@) {
    return unless @_;
    my $sob = new();
    $s_name 	 =    $sob->[0]     	     = $_[0];
    @s_aliases	 = @{ $sob->[1] } = split ' ', $_[1];
    $s_port	 =    $sob->[2] 	     = $_[2];
    $s_proto	 =    $sob->[3] 	     = $_[3];
    return $sob;
}

sub getservent    (   ) { populate(CORE::getservent()) }
sub getservbyname ($;$) { populate(CORE::getservbyname(shift,shift||'tcp')) }
sub getservbyport ($;$) { populate(CORE::getservbyport(shift,shift||'tcp')) }

sub getserv ($;$) {
    no strict 'refs';
    return &{'getservby' . ($_[0]=~/^\d+$/ ? 'port' : 'name')}(@_);
}

1;

__END__

=head1 NAME

Net::servent - by-name interface to Perl's built-in getserv*() functions

=head1 SYNOPSIS

 use Net::servent;
 $s = getservbyname(shift || 'ftp') || die "no service";
 printf "port for %s is %s, aliases are %s\n",
    $s->name, $s->port, "@{$s->aliases}";

 use Net::servent qw(:FIELDS);
 getservbyname(shift || 'ftp') || die "no service";
 print "port for $s_name is $s_port, aliases are @s_aliases\n";

=head1 DESCRIPTION

This module's default exports override the core getservent(),
getservbyname(), and
getnetbyport() functions, replacing them with versions that return
"Net::servent" objects.  They take default second arguments of "tcp".  This object has methods that return the similarly
named structure field name from the C's servent structure from F<netdb.h>;
namely name, aliases, port, and proto.  The aliases
method returns an array reference, the rest scalars.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as variables named
with a preceding C<s_>.  Thus, C<$serv_obj-E<gt>name()> corresponds to
$s_name if you import the fields.  Array references are available as
regular array variables, so for example C<@{ $serv_obj-E<gt>aliases()}>
would be simply @s_aliases.

The getserv() function is a simple front-end that forwards a numeric
argument to getservbyport(), and the rest to getservbyname().

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 EXAMPLES

 use Net::servent qw(:FIELDS);

 while (@ARGV) {
     my ($service, $proto) = ((split m!/!, shift), 'tcp');
     my $valet = getserv($service, $proto);
     unless ($valet) {
         warn "$0: No service: $service/$proto\n"
         next;
     }
     printf "service $service/$proto is port %d\n", $valet->port;
     print "alias are @s_aliases\n" if @s_aliases;
 }

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
