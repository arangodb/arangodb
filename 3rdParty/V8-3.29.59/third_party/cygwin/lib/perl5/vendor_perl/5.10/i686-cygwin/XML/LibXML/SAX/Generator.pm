# $Id: Generator.pm 709 2008-01-29 21:01:32Z pajas $

package XML::LibXML::SAX::Generator;

use strict;

use XML::LibXML;
use vars qw ($VERSION);

$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE

warn("This class (", __PACKAGE__, ") is deprecated!");

sub new {
    my $class = shift;
    unshift @_, 'Handler' unless @_ != 1;
    my %p = @_;
    return bless \%p, $class;
}

sub generate {
    my $self = shift;
    my ($node) = @_;
    
    my $document = { Parent => undef };
    $self->{Handler}->start_document($document);
    
    process_node($self->{Handler}, $node);
    
    $self->{Handler}->end_document($document);
}

sub process_node {
    my ($handler, $node) = @_;
    
    my $node_type = $node->getType();
    if ($node_type == XML_COMMENT_NODE) {
        $handler->comment( { Data => $node->getData } );
    }
    elsif ($node_type == XML_TEXT_NODE || $node_type == XML_CDATA_SECTION_NODE) {
        # warn($node->getData . "\n");
        $handler->characters( { Data => $node->getData } );
    }
    elsif ($node_type == XML_ELEMENT_NODE) {
        # warn("<" . $node->getName . ">\n");
        process_element($handler, $node);
        # warn("</" . $node->getName . ">\n");
    }
    elsif ($node_type == XML_ENTITY_REF_NODE) {
        foreach my $kid ($node->getChildnodes) {
            # warn("child of entity ref: " . $kid->getType() . " called: " . $kid->getName . "\n");
            process_node($handler, $kid);
        }
    }
    elsif ($node_type == XML_DOCUMENT_NODE) {
        # just get root element. Ignore other cruft.
        foreach my $kid ($node->getChildnodes) {
            if ($kid->getType() == XML_ELEMENT_NODE) {
                process_element($handler, $kid);
                last;
            }
        }
    }
    else {
        warn("unknown node type: $node_type");
    }
}

sub process_element {
    my ($handler, $element) = @_;
    
    my @attr;
    
    foreach my $attr ($element->getAttributes) {
        push @attr, XML::LibXML::SAX::AttributeNode->new(
            Name => $attr->getName,
            Value => $attr->getData,
            NamespaceURI => $attr->getNamespaceURI,
            Prefix => $attr->getPrefix,
            LocalName => $attr->getLocalName,
            );
    }
    
    my $node = {
        Name => $element->getName,
        Attributes => { map { $_->{Name} => $_ } @attr },
        NamespaceURI => $element->getNamespaceURI,
        Prefix => $element->getPrefix,
        LocalName => $element->getLocalName,
    };
    
    $handler->start_element($node);
    
    foreach my $child ($element->getChildnodes) {
        process_node($handler, $child);
    }
    
    $handler->end_element($node);
}

package XML::LibXML::SAX::AttributeNode;

use overload '""' => "stringify";

sub new {
    my $class = shift;
    my %p = @_;
    return bless \%p, $class;
}

sub stringify {
    my $self = shift;
    return $self->{Value};
}

1;

__END__

=head1 NAME

XML::LibXML::SAX::Generator - Generate SAX events from a LibXML tree

=head1 SYNOPSIS

  my $handler = MySAXHandler->new();
  my $generator = XML::LibXML::SAX::Generator->new(Handler => $handler);
  my $dom = XML::LibXML->new->parse_file("foo.xml");
  
  $generator->generate($dom);

=head1 DESCRIPTION

THIS CLASS IS DEPRACED! Use XML::LibXML::SAX::Parser instead!

This helper class allows you to generate SAX events from any XML::LibXML
node, and all it's sub-nodes. This basically gives you interop from
XML::LibXML to other modules that may implement SAX.

It uses SAX2 style, but should be compatible with anything SAX1, by use
of stringification overloading.

There is nothing to really know about, beyond the synopsis above, and
a general knowledge of how to use SAX, which is beyond the scope here.

=cut
