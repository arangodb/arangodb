package URI::gopher;  # <draft-murali-url-gopher>, Dec 4, 1996

require URI::_server;
@ISA=qw(URI::_server);

use strict;
use URI::Escape qw(uri_unescape);

#  A Gopher URL follows the common internet scheme syntax as defined in 
#  section 4.3 of [RFC-URL-SYNTAX]:
#
#        gopher://<host>[:<port>]/<gopher-path>
#
#  where
#
#        <gopher-path> :=  <gopher-type><selector> | 
#                          <gopher-type><selector>%09<search> |
#                          <gopher-type><selector>%09<search>%09<gopher+_string>
#
#        <gopher-type> := '0' | '1' | '2' | '3' | '4' | '5' | '6' | '7'
#                         '8' | '9' | '+' | 'I' | 'g' | 'T'
#
#        <selector>    := *pchar     Refer to RFC 1808 [4]
#        <search>      := *pchar
#        <gopher+_string> := *uchar  Refer to RFC 1738 [3]
#        
#  If the optional port is omitted, the port defaults to 70. 

sub default_port { 70 }

sub _gopher_type
{
    my $self = shift;
    my $path = $self->path_query;
    $path =~ s,^/,,;
    my $gtype = $1 if $path =~ s/^(.)//s;
    if (@_) {
	my $new_type = shift;
	if (defined($new_type)) {
	    Carp::croak("Bad gopher type '$new_type'")
               unless length($new_type) == 1;
	    substr($path, 0, 0) = $new_type;
	    $self->path_query($path);
	} else {
	    Carp::croak("Can't delete gopher type when selector is present")
		if length($path);
	    $self->path_query(undef);
	}
    }
    return $gtype;
}

sub gopher_type
{
    my $self = shift;
    my $gtype = $self->_gopher_type(@_);
    $gtype = "1" unless defined $gtype;
    $gtype;
}

*gtype = \&gopher_type;  # URI::URL compatibility

sub selector { shift->_gfield(0, @_) }
sub search   { shift->_gfield(1, @_) }
sub string   { shift->_gfield(2, @_) }

sub _gfield
{
    my $self = shift;
    my $fno  = shift;
    my $path = $self->path_query;

    # not according to spec., but many popular browsers accept
    # gopher URLs with a '?' before the search string.
    $path =~ s/\?/\t/;
    $path = uri_unescape($path);
    $path =~ s,^/,,;
    my $gtype = $1 if $path =~ s,^(.),,s;
    my @path = split(/\t/, $path, 3);
    if (@_) {
	# modify
	my $new = shift;
	$path[$fno] = $new;
	pop(@path) while @path && !defined($path[-1]);
	for (@path) { $_="" unless defined }
	$path = $gtype;
	$path = "1" unless defined $path;
	$path .= join("\t", @path);
	$self->path_query($path);
    }
    $path[$fno];
}

1;
