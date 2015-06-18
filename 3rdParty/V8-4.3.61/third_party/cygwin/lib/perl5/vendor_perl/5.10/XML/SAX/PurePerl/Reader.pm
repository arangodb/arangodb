# $Id: Reader.pm,v 1.11 2005/10/14 20:31:20 matt Exp $

package XML::SAX::PurePerl::Reader;

use strict;
use XML::SAX::PurePerl::Reader::URI;
use XML::SAX::PurePerl::Productions qw( $SingleChar $Letter $NameChar );
use Exporter ();

use vars qw(@ISA @EXPORT_OK);
@ISA = qw(Exporter);
@EXPORT_OK = qw(
    EOF
    BUFFER
    LINE
    COLUMN
    ENCODING
    XML_VERSION
);

use constant EOF => 0;
use constant BUFFER => 1;
use constant LINE => 2;
use constant COLUMN => 3;
use constant ENCODING => 4;
use constant SYSTEM_ID => 5;
use constant PUBLIC_ID => 6;
use constant XML_VERSION => 7;

require XML::SAX::PurePerl::Reader::Stream;
require XML::SAX::PurePerl::Reader::String;

if ($] >= 5.007002) {
    require XML::SAX::PurePerl::Reader::UnicodeExt;
}
else {
    require XML::SAX::PurePerl::Reader::NoUnicodeExt;
}

sub new {
    my $class = shift;
    my $thing = shift;
    
    # try to figure if this $thing is a handle of some sort
    if (ref($thing) && UNIVERSAL::isa($thing, 'IO::Handle')) {
        return XML::SAX::PurePerl::Reader::Stream->new($thing)->init;
    }
    my $ioref;
    if (tied($thing)) {
        my $class = ref($thing);
        no strict 'refs';
        $ioref = $thing if defined &{"${class}::TIEHANDLE"};
    }
    else {
        eval {
            $ioref = *{$thing}{IO};
        };
        undef $@;
    }
    if ($ioref) {
        return XML::SAX::PurePerl::Reader::Stream->new($thing)->init;
    }
    
    if ($thing =~ /</) {
        # assume it's a string
        return XML::SAX::PurePerl::Reader::String->new($thing)->init;
    }
    
    # assume it is a    uri
    return XML::SAX::PurePerl::Reader::URI->new($thing)->init;
}

sub init {
    my $self = shift;
    $self->[LINE] = 1;
    $self->[COLUMN] = 1;
    $self->read_more;
    return $self;
}

sub data {
    my ($self, $min_length) = (@_, 1);
    if (length($self->[BUFFER]) < $min_length) {
        $self->read_more;
    }
    return $self->[BUFFER];
}

sub match {
    my ($self, $char) = @_;
    my $data = $self->data;
    if (substr($data, 0, 1) eq $char) {
        $self->move_along(1);
        return 1;
    }
    return 0;
}

sub public_id {
    my $self = shift;
    @_ and $self->[PUBLIC_ID] = shift;
    $self->[PUBLIC_ID];
}

sub system_id {
    my $self = shift;
    @_ and $self->[SYSTEM_ID] = shift;
    $self->[SYSTEM_ID];
}

sub line {
    shift->[LINE];
}

sub column {
    shift->[COLUMN];
}

sub get_encoding {
    my $self = shift;
    return $self->[ENCODING];
}

sub get_xml_version {
    my $self = shift;
    return $self->[XML_VERSION];
}

1;

__END__

=head1 NAME

XML::Parser::PurePerl::Reader - Abstract Reader factory class

=cut
