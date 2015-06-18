# $Id: DebugHandler.pm,v 1.3 2001/11/24 17:47:53 matt Exp $

package XML::SAX::PurePerl::DebugHandler;

use strict;

sub new {
    my $class = shift;
    my %opts = @_;
    return bless \%opts, $class;
}

# DocumentHandler

sub set_document_locator {
    my $self = shift;
    print "set_document_locator\n" if $ENV{DEBUG_XML};
    $self->{seen}{set_document_locator}++;
}

sub start_document {
    my $self = shift;
    print "start_document\n" if $ENV{DEBUG_XML};
    $self->{seen}{start_document}++;    
}

sub end_document {
    my $self = shift;
    print "end_document\n" if $ENV{DEBUG_XML};
    $self->{seen}{end_document}++;
}

sub start_element {
    my $self = shift;
    print "start_element\n" if $ENV{DEBUG_XML};
    $self->{seen}{start_element}++;
}

sub end_element {
    my $self = shift;
    print "end_element\n" if $ENV{DEBUG_XML};
    $self->{seen}{end_element}++;
}

sub characters {
    my $self = shift;
    print "characters\n" if $ENV{DEBUG_XML};
#    warn "Char: ", $_[0]->{Data}, "\n";
    $self->{seen}{characters}++;
}

sub processing_instruction {
    my $self = shift;
    print "processing_instruction\n" if $ENV{DEBUG_XML};
    $self->{seen}{processing_instruction}++;
}

sub ignorable_whitespace {
    my $self = shift;
    print "ignorable_whitespace\n" if $ENV{DEBUG_XML};
    $self->{seen}{ignorable_whitespace}++;
}

# LexHandler

sub comment {
    my $self = shift;
    print "comment\n" if $ENV{DEBUG_XML};
    $self->{seen}{comment}++;
}

# DTDHandler

sub notation_decl {
    my $self = shift;
    print "notation_decl\n" if $ENV{DEBUG_XML};
    $self->{seen}{notation_decl}++;
}

sub unparsed_entity_decl {
    my $self = shift;
    print "unparsed_entity_decl\n" if $ENV{DEBUG_XML};
    $self->{seen}{entity_decl}++;
}

# EntityResolver

sub resolve_entity {
    my $self = shift;
    print "resolve_entity\n" if $ENV{DEBUG_XML};
    $self->{seen}{resolve_entity}++;
    return '';
}

1;
