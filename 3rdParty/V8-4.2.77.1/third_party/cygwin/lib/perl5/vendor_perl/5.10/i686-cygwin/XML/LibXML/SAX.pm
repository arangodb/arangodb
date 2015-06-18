# $Id: SAX.pm 709 2008-01-29 21:01:32Z pajas $
# Copyright (c) 2001-2002, AxKit.com Ltd. All rights reserved.
package XML::LibXML::SAX;

use strict;
use vars qw($VERSION @ISA);

$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE

use XML::LibXML;
use XML::SAX::Base;

use base qw(XML::SAX::Base);

use Carp;
use IO::File;

sub _parse_characterstream {
    my ( $self, $fh ) = @_;
    # this my catch the xml decl, so the parser won't get confused about
    # a possibly wrong encoding.
    croak( "not implemented yet" );
}

sub _parse_bytestream {
    my ( $self, $fh ) = @_;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new;
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_fh;
    $self->{ParserOptions}{ParseFuncParam} = $fh;
    return $self->_parse;
}

sub _parse_string {
    my ( $self, $string ) = @_;
#    $self->{ParserOptions}{LibParser}      = XML::LibXML->new;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new()     unless defined $self->{ParserOptions}{LibParser};
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_string;
    $self->{ParserOptions}{ParseFuncParam} = $string;
    return $self->_parse;
}

sub _parse_systemid {
    my $self = shift;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new;
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_file;
    $self->{ParserOptions}{ParseFuncParam} = shift;
    return $self->_parse;
}

sub parse_chunk {
    my ( $self, $chunk ) = @_;
    $self->{ParserOptions}{LibParser}      = XML::LibXML->new;
    $self->{ParserOptions}{ParseFunc}      = \&XML::LibXML::parse_xml_chunk;
    $self->{ParserOptions}{ParseFuncParam} = $chunk;
    return $self->_parse;
}

sub _parse {
    my $self = shift;
    my $args = bless $self->{ParserOptions}, ref($self);

    $args->{LibParser}->set_handler( $self );
    eval {
      $args->{ParseFunc}->($args->{LibParser}, $args->{ParseFuncParam});
    };

    if ( $args->{LibParser}->{SAX}->{State} == 1 ) {
        croak( "SAX Exception not implemented, yet; Data ended before document ended\n" );
    }

    # break a possible circular reference    
    $args->{LibParser}->set_handler( undef );
    if ( $@ ) {
        croak $@;
    }
    return $self->end_document({});
}


1;

