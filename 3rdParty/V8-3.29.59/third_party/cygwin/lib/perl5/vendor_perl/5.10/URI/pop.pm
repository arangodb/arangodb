package URI::pop;   # RFC 2384

require URI::_server;
@ISA=qw(URI::_server);

use strict;
use URI::Escape qw(uri_unescape);

sub default_port { 110 }

#pop://<user>;auth=<auth>@<host>:<port>

sub user
{
    my $self = shift;
    my $old = $self->userinfo;

    if (@_) {
	my $new_info = $old;
	$new_info = "" unless defined $new_info;
	$new_info =~ s/^[^;]*//;

	my $new = shift;
	if (!defined($new) && !length($new_info)) {
	    $self->userinfo(undef);
	} else {
	    $new = "" unless defined $new;
	    $new =~ s/%/%25/g;
	    $new =~ s/;/%3B/g;
	    $self->userinfo("$new$new_info");
	}
    }

    return unless defined $old;
    $old =~ s/;.*//;
    return uri_unescape($old);
}

sub auth
{
    my $self = shift;
    my $old = $self->userinfo;

    if (@_) {
	my $new = $old;
	$new = "" unless defined $new;
	$new =~ s/(^[^;]*)//;
	my $user = $1;
	$new =~ s/;auth=[^;]*//i;

	
	my $auth = shift;
	if (defined $auth) {
	    $auth =~ s/%/%25/g;
	    $auth =~ s/;/%3B/g;
	    $new = ";AUTH=$auth$new";
	}
	$self->userinfo("$user$new");
	
    }

    return unless defined $old;
    $old =~ s/^[^;]*//;
    return uri_unescape($1) if $old =~ /;auth=(.*)/i;
    return;
}

1;
