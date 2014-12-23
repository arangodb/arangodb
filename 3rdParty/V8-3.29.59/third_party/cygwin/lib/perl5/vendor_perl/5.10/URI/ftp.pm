package URI::ftp;

require URI::_server;
require URI::_userpass;
@ISA=qw(URI::_server URI::_userpass);

use strict;

sub default_port { 21 }

sub path { shift->path_query(@_) }  # XXX

sub _user     { shift->SUPER::user(@_);     }
sub _password { shift->SUPER::password(@_); }

sub user
{
    my $self = shift;
    my $user = $self->_user(@_);
    $user = "anonymous" unless defined $user;
    $user;
}

sub password
{
    my $self = shift;
    my $pass = $self->_password(@_);
    unless (defined $pass) {
	my $user = $self->user;
	if ($user eq 'anonymous' || $user eq 'ftp') {
	    # anonymous ftp login password
            # If there is no ftp anonymous password specified
            # then we'll just use 'anonymous@'
            # We don't try to send the read e-mail address because:
            # - We want to remain anonymous
            # - We want to stop SPAM
            # - We don't want to let ftp sites to discriminate by the user,
            #   host, country or ftp client being used.
	    $pass = 'anonymous@';
	}
    }
    $pass;
}

1;
