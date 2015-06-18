# $Id: Parser.pm 709 2008-01-29 21:01:32Z pajas $

package XML::LibXML::SAX::Parser;

use strict;
use vars qw($VERSION @ISA);

use XML::LibXML;
use XML::LibXML::Common qw(:libxml);
use XML::SAX::Base;
use XML::SAX::DocumentLocator;

$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE
@ISA = ('XML::SAX::Base');

sub _parse_characterstream {
    my ($self, $fh, $options) = @_;
    die "parsing a characterstream is not supported at this time";
}

sub _parse_bytestream {
    my ($self, $fh, $options) = @_;
    my $parser = XML::LibXML->new();
    my $doc = exists($options->{Source}{SystemId}) ? $parser->parse_fh($fh, $options->{Source}{SystemId}) : $parser->parse_fh($fh);
    $self->generate($doc);
}

sub _parse_string {
    my ($self, $str, $options) = @_;
    my $parser = XML::LibXML->new();
    my $doc = exists($options->{Source}{SystemId}) ? $parser->parse_string($str, $options->{Source}{SystemId}) : $parser->parse_string($str);
    $self->generate($doc);
}

sub _parse_systemid {
    my ($self, $sysid, $options) = @_;
    my $parser = XML::LibXML->new();
    my $doc = $parser->parse_file($sysid);
    $self->generate($doc);
}

sub generate {
    my $self = shift;
    my ($node) = @_;

    my $doc = $node->ownerDocument();
    {
      # precompute some DocumentLocator values
      my %locator = (
	PublicId => undef,
	SystemId => undef,
	Encoding => undef,
	XMLVersion => undef,
       );
      my $dtd = defined $doc ? $doc->externalSubset() : undef;
      if (defined $dtd) {
	$locator{PublicId} = $dtd->publicId();
	$locator{SystemId} = $dtd->systemId();
      }
      if (defined $doc) {
	$locator{Encoding} = $doc->encoding();
	$locator{XMLVersion} = $doc->version();
      }
      $self->set_document_locator(
	XML::SAX::DocumentLocator->new(
	  sub { $locator{PublicId} },
	  sub { $locator{SystemId} },
	  sub { defined($self->{current_node}) ? $self->{current_node}->line_number() : undef },
	  sub { 1 },
	  sub { $locator{Encoding} },
	  sub { $locator{XMLVersion} },
	 ),
       );
    }

    if ( $node->nodeType() == XML_DOCUMENT_NODE
         || $node->nodeType == XML_HTML_DOCUMENT_NODE ) {
        $self->start_document({});
        $self->xml_decl({Version => $node->getVersion, Encoding => $node->getEncoding});
        $self->process_node($node);
        $self->end_document({});
    }
}

sub process_node {
    my ($self, $node) = @_;

    local $self->{current_node} = $node;

    my $node_type = $node->nodeType();
    if ($node_type == XML_COMMENT_NODE) {
        $self->comment( { Data => $node->getData } );
    }
    elsif ($node_type == XML_TEXT_NODE
           || $node_type == XML_CDATA_SECTION_NODE) {
        # warn($node->getData . "\n");
        $self->characters( { Data => $node->nodeValue } );
    }
    elsif ($node_type == XML_ELEMENT_NODE) {
        # warn("<" . $node->getName . ">\n");
        $self->process_element($node);
        # warn("</" . $node->getName . ">\n");
    }
    elsif ($node_type == XML_ENTITY_REF_NODE) {
        foreach my $kid ($node->childNodes) {
            # warn("child of entity ref: " . $kid->getType() . " called: " . $kid->getName . "\n");
            $self->process_node($kid);
        }
    }
    elsif ($node_type == XML_DOCUMENT_NODE
           || $node_type == XML_HTML_DOCUMENT_NODE
           || $node_type == XML_DOCUMENT_FRAG_NODE) {
        # some times it is just usefull to generate SAX events from
        # a document fragment (very good with filters).
        foreach my $kid ($node->childNodes) {
            $self->process_node($kid);
        }
    }
    elsif ($node_type == XML_PI_NODE) {
        $self->processing_instruction( { Target =>  $node->getName, Data => $node->getData } );
    }
    elsif ($node_type == XML_COMMENT_NODE) {
        $self->comment( { Data => $node->getData } );
    }
    elsif ( $node_type == XML_XINCLUDE_START
            || $node_type == XML_XINCLUDE_END ) {
        # ignore!
        # i may want to handle this one day, dunno yet
    }
    elsif ($node_type == XML_DTD_NODE ) {
        # ignore!
        # i will support DTDs, but had no time yet.
    }
    else {
        # warn("unsupported node type: $node_type");
    }

}

sub process_element {
    my ($self, $element) = @_;

    my $attribs = {};
    my @ns_maps = $element->getNamespaces;

    foreach my $ns (@ns_maps) {
        $self->start_prefix_mapping(
            {
                NamespaceURI => $ns->href,
                Prefix       => ( defined $ns->localname  ? $ns->localname : ''),
            }
        );
    }

    foreach my $attr ($element->attributes) {
        my $key;
        # warn("Attr: $attr -> ", $attr->getName, " = ", $attr->getData, "\n");
        # this isa dump thing...
        if ($attr->isa('XML::LibXML::Namespace')) {
            # TODO This needs fixing modulo agreeing on what
            # is the right thing to do here.
            unless ( defined $attr->name ) {
                ## It's an atter like "xmlns='foo'"
                $attribs->{"{}xmlns"} =
                  {     
                   Name         => "xmlns",
                   LocalName    => "xmlns",
                   Prefix       => "",     
                   Value        => $attr->href,
                   NamespaceURI => "",
                  };
            }
            else {
                my $prefix = "xmlns";
                my $localname = $attr->localname;
                my $key = "{http://www.w3.org/2000/xmlns/}";
                my $name = "xmlns";

                if ( defined $localname ) {
                    $key .= $localname;
                    $name.= ":".$localname;
                }

                $attribs->{$key} =
                  {
                   Name         => $name,
                   Value        => $attr->href,
                   NamespaceURI => "http://www.w3.org/2000/xmlns/",
                   Prefix       => $prefix,
                   LocalName    => $localname,
                  };
            }
        }
        else {
            my $ns = $attr->namespaceURI;

            $ns = '' unless defined $ns;
            $key = "{$ns}".$attr->localname;
            ## Not sure why, but $attr->name is coming through stripped
            ## of its prefix, so we need to hand-assemble a real name.
            my $name = $attr->name;
            $name = "" unless defined $name;

            my $prefix = $attr->prefix;
            $prefix = "" unless defined $prefix;
            $name = "$prefix:$name"
              if index( $name, ":" ) < 0 && length $prefix;

            $attribs->{$key} =
                {
                    Name => $name,
                    Value => $attr->value,
                    NamespaceURI => $ns,
                    Prefix => $prefix,
                    LocalName => $attr->localname,
                };
        }
        # use Data::Dumper;
        # warn("Attr made: ", Dumper($attribs->{$key}), "\n");
    }

    my $node = {
        Name => $element->nodeName,
        Attributes => $attribs,
        NamespaceURI => $element->namespaceURI,
        Prefix => $element->prefix || "",
        LocalName => $element->localname,
    };

    $self->start_element($node);

    foreach my $child ($element->childNodes) {
        $self->process_node($child);
    }

    my $end_node = { %$node };

    delete $end_node->{Attributes};

    $self->end_element($end_node);

    foreach my $ns (@ns_maps) {
        $self->end_prefix_mapping(
            {
                NamespaceURI => $ns->href,
                Prefix       => ( defined $ns->localname  ? $ns->localname : ''),
            }
        );
    }
}

1;

__END__
