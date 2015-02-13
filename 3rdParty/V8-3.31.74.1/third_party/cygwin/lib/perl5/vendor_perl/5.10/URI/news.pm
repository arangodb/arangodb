package URI::news;  # draft-gilman-news-url-01

require URI::_server;
@ISA=qw(URI::_server);

use strict;
use URI::Escape qw(uri_unescape);
use Carp ();

sub default_port { 119 }

#   newsURL      =  scheme ":" [ news-server ] [ refbygroup | message ]
#   scheme       =  "news" | "snews" | "nntp"
#   news-server  =  "//" server "/"
#   refbygroup   = group [ "/" messageno [ "-" messageno ] ]
#   message      = local-part "@" domain

sub _group
{
    my $self = shift;
    my $old = $self->path;
    if (@_) {
	my($group,$from,$to) = @_;
	if ($group =~ /\@/) {
            $group =~ s/^<(.*)>$/$1/;  # "<" and ">" should not be part of it
	}
	$group =~ s,%,%25,g;
	$group =~ s,/,%2F,g;
	my $path = $group;
	if (defined $from) {
	    $path .= "/$from";
	    $path .= "-$to" if defined $to;
	}
	$self->path($path);
    }

    $old =~ s,^/,,;
    if ($old !~ /\@/ && $old =~ s,/(.*),, && wantarray) {
	my $extra = $1;
	return (uri_unescape($old), split(/-/, $extra));
    }
    uri_unescape($old);
}


sub group
{
    my $self = shift;
    if (@_) {
	Carp::croak("Group name can't contain '\@'") if $_[0] =~ /\@/;
    }
    my @old = $self->_group(@_);
    return if $old[0] =~ /\@/;
    wantarray ? @old : $old[0];
}

sub message
{
    my $self = shift;
    if (@_) {
	Carp::croak("Message must contain '\@'") unless $_[0] =~ /\@/;
    }
    my $old = $self->_group(@_);
    return unless $old =~ /\@/;
    return $old;
}

1;
