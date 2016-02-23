package URI::_server;
require URI::_generic;
@ISA=qw(URI::_generic);

use strict;
use URI::Escape qw(uri_unescape);

sub userinfo
{
    my $self = shift;
    my $old = $self->authority;

    if (@_) {
	my $new = $old;
	$new = "" unless defined $new;
	$new =~ s/.*@//;  # remove old stuff
	my $ui = shift;
	if (defined $ui) {
	    $ui =~ s/@/%40/g;   # protect @
	    $new = "$ui\@$new";
	}
	$self->authority($new);
    }
    return undef if !defined($old) || $old !~ /(.*)@/;
    return $1;
}

sub host
{
    my $self = shift;
    my $old = $self->authority;
    if (@_) {
	my $tmp = $old;
	$tmp = "" unless defined $tmp;
	my $ui = ($tmp =~ /(.*@)/) ? $1 : "";
	my $port = ($tmp =~ /(:\d+)$/) ? $1 : "";
	my $new = shift;
	$new = "" unless defined $new;
	if (length $new) {
	    $new =~ s/[@]/%40/g;   # protect @
	    $port = $1 if $new =~ s/(:\d+)$//;
	}
	$self->authority("$ui$new$port");
    }
    return undef unless defined $old;
    $old =~ s/.*@//;
    $old =~ s/:\d+$//;
    return uri_unescape($old);
}

sub _port
{
    my $self = shift;
    my $old = $self->authority;
    if (@_) {
	my $new = $old;
	$new =~ s/:\d*$//;
	my $port = shift;
	$new .= ":$port" if defined $port;
	$self->authority($new);
    }
    return $1 if defined($old) && $old =~ /:(\d*)$/;
    return;
}

sub port
{
    my $self = shift;
    my $port = $self->_port(@_);
    $port = $self->default_port if !defined($port) || $port eq "";
    $port;
}

sub host_port
{
    my $self = shift;
    my $old = $self->authority;
    $self->host(shift) if @_;
    return undef unless defined $old;
    $old =~ s/.*@//;        # zap userinfo
    $old =~ s/:$//;         # empty port does not could
    $old .= ":" . $self->port unless $old =~ /:/;
    $old;
}


sub default_port { undef }

sub canonical
{
    my $self = shift;
    my $other = $self->SUPER::canonical;
    my $host = $other->host || "";
    my $port = $other->_port;
    my $uc_host = $host =~ /[A-Z]/;
    my $def_port = defined($port) && ($port eq "" ||
                                      $port == $self->default_port);
    if ($uc_host || $def_port) {
	$other = $other->clone if $other == $self;
	$other->host(lc $host) if $uc_host;
	$other->port(undef)    if $def_port;
    }
    $other;
}

1;
