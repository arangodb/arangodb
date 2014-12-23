package Net::hostent;
use strict;

use 5.006_001;
our $VERSION = '1.01';
our(@EXPORT, @EXPORT_OK, %EXPORT_TAGS);
BEGIN { 
    use Exporter   ();
    @EXPORT      = qw(gethostbyname gethostbyaddr gethost);
    @EXPORT_OK   = qw(
			$h_name	    	@h_aliases
			$h_addrtype 	$h_length
			@h_addr_list 	$h_addr
		   );
    %EXPORT_TAGS = ( FIELDS => [ @EXPORT_OK, @EXPORT ] );
}
use vars      @EXPORT_OK;

# Class::Struct forbids use of @ISA
sub import { goto &Exporter::import }

use Class::Struct qw(struct);
struct 'Net::hostent' => [
   name		=> '$',
   aliases	=> '@',
   addrtype	=> '$',
   'length'	=> '$',
   addr_list	=> '@',
];

sub addr { shift->addr_list->[0] }

sub populate (@) {
    return unless @_;
    my $hob = new();
    $h_name 	 =    $hob->[0]     	     = $_[0];
    @h_aliases	 = @{ $hob->[1] } = split ' ', $_[1];
    $h_addrtype  =    $hob->[2] 	     = $_[2];
    $h_length	 =    $hob->[3] 	     = $_[3];
    $h_addr 	 =                             $_[4];
    @h_addr_list = @{ $hob->[4] } =          @_[ (4 .. $#_) ];
    return $hob;
} 

sub gethostbyname ($)  { populate(CORE::gethostbyname(shift)) } 

sub gethostbyaddr ($;$) { 
    my ($addr, $addrtype);
    $addr = shift;
    require Socket unless @_;
    $addrtype = @_ ? shift : Socket::AF_INET();
    populate(CORE::gethostbyaddr($addr, $addrtype)) 
} 

sub gethost($) {
    if ($_[0] =~ /^\d+(?:\.\d+(?:\.\d+(?:\.\d+)?)?)?$/) {
	require Socket;
	&gethostbyaddr(Socket::inet_aton(shift));
    } else {
	&gethostbyname;
    } 
} 

1;
__END__

=head1 NAME

Net::hostent - by-name interface to Perl's built-in gethost*() functions

=head1 SYNOPSIS

 use Net::hostent;

=head1 DESCRIPTION

This module's default exports override the core gethostbyname() and
gethostbyaddr() functions, replacing them with versions that return
"Net::hostent" objects.  This object has methods that return the similarly
named structure field name from the C's hostent structure from F<netdb.h>;
namely name, aliases, addrtype, length, and addr_list.  The aliases and
addr_list methods return array reference, the rest scalars.  The addr
method is equivalent to the zeroth element in the addr_list array
reference.

You may also import all the structure fields directly into your namespace
as regular variables using the :FIELDS import tag.  (Note that this still
overrides your core functions.)  Access these fields as variables named
with a preceding C<h_>.  Thus, C<$host_obj-E<gt>name()> corresponds to
$h_name if you import the fields.  Array references are available as
regular array variables, so for example C<@{ $host_obj-E<gt>aliases()
}> would be simply @h_aliases.

The gethost() function is a simple front-end that forwards a numeric
argument to gethostbyaddr() by way of Socket::inet_aton, and the rest
to gethostbyname().

To access this functionality without the core overrides,
pass the C<use> an empty import list, and then access
function functions with their full qualified names.
On the other hand, the built-ins are still available
via the C<CORE::> pseudo-package.

=head1 EXAMPLES

 use Net::hostent;
 use Socket;

 @ARGV = ('netscape.com') unless @ARGV;

 for $host ( @ARGV ) {

    unless ($h = gethost($host)) {
	warn "$0: no such host: $host\n";
	next;
    }

    printf "\n%s is %s%s\n", 
	    $host, 
	    lc($h->name) eq lc($host) ? "" : "*really* ",
	    $h->name;

    print "\taliases are ", join(", ", @{$h->aliases}), "\n"
		if @{$h->aliases};     

    if ( @{$h->addr_list} > 1 ) { 
	my $i;
	for $addr ( @{$h->addr_list} ) {
	    printf "\taddr #%d is [%s]\n", $i++, inet_ntoa($addr);
	} 
    } else {
	printf "\taddress is [%s]\n", inet_ntoa($h->addr);
    } 

    if ($h = gethostbyaddr($h->addr)) {
	if (lc($h->name) ne lc($host)) {
	    printf "\tThat addr reverses to host %s!\n", $h->name;
	    $host = $h->name;
	    redo;
	} 
    }
 }

=head1 NOTE

While this class is currently implemented using the Class::Struct
module to build a struct-like class, you shouldn't rely upon this.

=head1 AUTHOR

Tom Christiansen
