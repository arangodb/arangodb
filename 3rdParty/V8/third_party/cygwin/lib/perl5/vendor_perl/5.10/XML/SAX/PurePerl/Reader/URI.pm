# $Id: URI.pm,v 1.1 2001/11/11 18:41:51 matt Exp $

package XML::SAX::PurePerl::Reader::URI;

use strict;

use XML::SAX::PurePerl::Reader;
use File::Temp qw(tempfile);
use Symbol;

## NOTE: This is *not* a subclass of Reader. It just returns Stream or String
## Reader objects depending on what it's capabilities are.

sub new {
    my $class = shift;
    my $uri = shift;
    # request the URI
    if (-e $uri && -f _) {
        my $fh = gensym;
        open($fh, $uri) || die "Cannot open file $uri : $!";
        return XML::SAX::PurePerl::Reader::Stream->new($fh);
    }
    elsif ($uri =~ /^file:(.*)$/ && -e $1 && -f _) {
        my $file = $1;
        my $fh = gensym;
        open($fh, $file) || die "Cannot open file $file : $!";
        return XML::SAX::PurePerl::Reader::Stream->new($fh);
    }
    else {
        # request URI, return String reader
        require LWP::UserAgent;
        my $ua = LWP::UserAgent->new;
        $ua->agent("Perl/XML/SAX/PurePerl/1.0 " . $ua->agent);
        
        my $req = HTTP::Request->new(GET => $uri);
        
        my $fh = tempfile();
        
        my $callback = sub {
            my ($data, $response, $protocol) = @_;
            print $fh $data;
        };
        
        my $res = $ua->request($req, $callback, 4096);
        
        if ($res->is_success) {
            seek($fh, 0, 0);
            return XML::SAX::PurePerl::Reader::Stream->new($fh);
        }
        else {
            die "LWP Request Failed";
        }
    }
}


1;
