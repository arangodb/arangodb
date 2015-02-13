#
# Written by Ryan Kereliuk <ryker@ryker.org>.  This file may be
# distributed under the same terms as Perl itself.
#
# The RFC 3261 sip URI is <scheme>:<authority>;<params>?<query>.
#

package URI::sip;

require URI::_server;
require URI::_userpass;
@ISA=qw(URI::_server URI::_userpass);

use strict;
use vars qw(@ISA $VERSION);
use URI::Escape qw(uri_unescape);

$VERSION = "0.10";

sub default_port { 5060 }

sub authority
{
    my $self = shift;
    $$self =~ m,^($URI::scheme_re:)?([^;?]*)(.*)$,os or die;
    my $old = $2;

    if (@_) {
        my $auth = shift;
        $$self = defined($1) ? $1 : "";
        my $rest = $3;
        if (defined $auth) {
            $auth =~ s/([^$URI::uric])/ URI::Escape::escape_char($1)/ego;
            $$self .= "$auth";
        }
        $$self .= $rest;
    }
    $old;
}

sub params_form
{
    my $self = shift;
    $$self =~ m,^((?:$URI::scheme_re:)?)(?:([^;?]*))?(;[^?]*)?(.*)$,os or die;
    my $paramstr = $3;

    if (@_) {
    	my @args = @_; 
        $$self = $1 . $2;
        my $rest = $4;
	my @new;
	for (my $i=0; $i < @args; $i += 2) {
	    push(@new, "$args[$i]=$args[$i+1]");
	}
	$paramstr = join(";", @new);
	$$self .= ";" . $paramstr . $rest;
    }
    $paramstr =~ s/^;//o;
    return split(/[;=]/, $paramstr);
}

sub params
{
    my $self = shift;
    $$self =~ m,^((?:$URI::scheme_re:)?)(?:([^;?]*))?(;[^?]*)?(.*)$,os or die;
    my $paramstr = $3;

    if (@_) {
    	my $new = shift; 
        $$self = $1 . $2;
        my $rest = $4;
	$$self .= $paramstr . $rest;
    }
    $paramstr =~ s/^;//o;
    return $paramstr;
}

# Inherited methods that make no sense for a SIP URI.
sub path {}
sub path_query {}
sub path_segments {}
sub abs { shift }
sub rel { shift }
sub query_keywords {}

1;
