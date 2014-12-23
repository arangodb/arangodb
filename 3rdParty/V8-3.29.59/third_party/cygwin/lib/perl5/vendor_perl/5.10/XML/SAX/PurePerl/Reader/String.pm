# $Id: String.pm,v 1.5 2003/07/30 13:39:23 matt Exp $

package XML::SAX::PurePerl::Reader::String;

use strict;
use vars qw(@ISA);

use XML::SAX::PurePerl::Reader qw(
    LINE
    COLUMN
    BUFFER
    ENCODING
    EOF
);

@ISA = ('XML::SAX::PurePerl::Reader');

use constant DISCARDED => 7;

sub new {
    my $class = shift;
    my $string = shift;
    my @parts;
    @parts[BUFFER, EOF, LINE, COLUMN, DISCARDED] =
        ($string,   0,   1,    0,       '');
    return bless \@parts, $class;
}

sub read_more () { }

sub move_along {
    my $self = shift;
    my $discarded = substr($self->[BUFFER], 0, $_[0], '');
    $self->[DISCARDED] .= $discarded;
    
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

    XML::SAX::PurePerl::Reader::switch_encoding_string($self->[BUFFER], $encoding, "utf-8");
    $self->[ENCODING] = $encoding;
}

sub bytepos {
    my $self = shift;
    length($self->[DISCARDED]);
}

1;
