# $Id: Stream.pm,v 1.7 2005/10/14 20:31:20 matt Exp $

package XML::SAX::PurePerl::Reader::Stream;

use strict;
use vars qw(@ISA);

use XML::SAX::PurePerl::Reader qw(
    EOF
    BUFFER
    LINE
    COLUMN
    ENCODING
    XML_VERSION
);
use XML::SAX::Exception;

@ISA = ('XML::SAX::PurePerl::Reader');

# subclassed by adding 1 to last element
use constant FH => 8;
use constant BUFFER_SIZE => 4096;

sub new {
    my $class = shift;
    my $ioref = shift;
    XML::SAX::PurePerl::Reader::set_raw_stream($ioref);
    my @parts;
    @parts[FH, LINE, COLUMN, BUFFER, EOF, XML_VERSION] =
        ($ioref, 1,   0,      '',     0,   '1.0');
    return bless \@parts, $class;
}

sub read_more {
    my $self = shift;
    my $buf;
    my $bytesread = read($self->[FH], $buf, BUFFER_SIZE);
    if ($bytesread) {
        $self->[BUFFER] .= $buf;
        return 1;
    }
    elsif (defined($bytesread)) {
        $self->[EOF]++;
        return 0;
    }
    else {
        throw XML::SAX::Exception::Parse(
            Message => "Error reading from filehandle: $!",
        );
    }
}

sub move_along {
    my $self = shift;
    my $discarded = substr($self->[BUFFER], 0, $_[0], '');
    
    # Wish I could skip this lot - tells us where we are in the file
    my $lines = $discarded =~ tr/\n//;
    $self->[LINE] += $lines;
    if ($lines) {
        $discarded =~ /\n([^\n]*)$/;
        $self->[COLUMN] = length($1);
    }
    else {
        $self->[COLUMN] += $_[0];
    }
}

sub set_encoding {
    my $self = shift;
    my ($encoding) = @_;
    # warn("set encoding to: $encoding\n");
    XML::SAX::PurePerl::Reader::switch_encoding_stream($self->[FH], $encoding);
    XML::SAX::PurePerl::Reader::switch_encoding_string($self->[BUFFER], $encoding);
    $self->[ENCODING] = $encoding;
}

sub bytepos {
    my $self = shift;
    tell($self->[FH]);
}

1;

