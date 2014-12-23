# $Id: Builder.pm 709 2008-01-29 21:01:32Z pajas $

package XML::LibXML::SAX::Builder;

use XML::LibXML;
use XML::NamespaceSupport;

use vars qw ($VERSION);

$VERSION = "1.66"; # VERSION TEMPLATE: DO NOT CHANGE

sub new {
    my $class = shift;
    return bless {@_}, $class;
}

sub result { $_[0]->{LAST_DOM}; }

sub done {
    my ($self) = @_;
    my $dom = $self->{DOM};
    $dom = $self->{Parent} unless defined $dom; # this is for parsing document chunks

    delete $self->{NamespaceStack};
    delete $self->{Parent};
    delete $self->{DOM};

    $self->{LAST_DOM} = $dom;

    return $dom;
}

sub set_document_locator {
}

sub start_dtd {
  my ($self, $dtd) = @_;
  if (defined $dtd->{Name} and
      (defined $dtd->{SystemID} or defined $dtd->{PublicID})) {
    $self->{DOM}->createExternalSubset($dtd->{Name},$dtd->{PublicID},$dtd->{SystemID});
  }
}

sub end_dtd {
}

sub start_document {
    my ($self, $doc) = @_;

    $self->{DOM} = XML::LibXML::Document->createDocument();

    if ( defined $self->{Encoding} ) {
        $self->xml_decl({Version => ($self->{Version} || '1.0') , Encoding => $self->{Encoding}});
    }

    $self->{NamespaceStack} = XML::NamespaceSupport->new;
    $self->{NamespaceStack}->push_context;
    $self->{Parent} = undef;
    return ();
}

sub xml_decl {
    my $self = shift;
    my $decl = shift;

    if ( defined $decl->{Version} ) {
        $self->{DOM}->setVersion( $decl->{Version} );
    }
    if ( defined $decl->{Encoding} ) {
        $self->{DOM}->setEncoding( $decl->{Encoding} );
    }
    return ();
}

sub end_document {
    my ($self, $doc) = @_;
    my $d = $self->done();
    return $d;
}

sub start_prefix_mapping {
    my $self = shift;
    my $ns = shift;

    unless ( defined $self->{DOM} or defined $self->{Parent} ) {
        $self->{Parent} = XML::LibXML::DocumentFragment->new();
        $self->{NamespaceStack} = XML::NamespaceSupport->new;
        $self->{NamespaceStack}->push_context;
    }

    $self->{USENAMESPACESTACK} = 1;

    $self->{NamespaceStack}->declare_prefix( $ns->{Prefix}, $ns->{NamespaceURI} );
    return ();
}


sub end_prefix_mapping {
    my $self = shift;
    my $ns = shift;
    $self->{NamespaceStack}->undeclare_prefix( $ns->{Prefix} );
    return ();
}


sub start_element {
    my ($self, $el) = @_;
    my $node;

    unless ( defined $self->{DOM} or defined $self->{Parent} ) {
        $self->{Parent} = XML::LibXML::DocumentFragment->new();
        $self->{NamespaceStack} = XML::NamespaceSupport->new;
        $self->{NamespaceStack}->push_context;
    }

    if ( defined $self->{Parent} ) {
        $el->{NamespaceURI} ||= "";
        $node = $self->{Parent}->addNewChild( $el->{NamespaceURI},
                                              $el->{Name} );
    }
    else {
        if ($el->{NamespaceURI}) {
            if ( defined $self->{DOM} ) {
                $node = $self->{DOM}->createRawElementNS($el->{NamespaceURI},
                                                         $el->{Name});
            }
            else {
                $node = XML::LibXML::Element->new( $el->{Name} );
                $node->setNamespace( $el->{NamespaceURI},
                                     $el->{Prefix} , 1 );
            }
        }
        else {
            if ( defined $self->{DOM} ) {
                $node = $self->{DOM}->createRawElement($el->{Name});
            }
            else {
                $node = XML::LibXML::Element->new( $el->{Name} );
            }
        }

        $self->{DOM}->setDocumentElement($node);
    }

    # build namespaces
    my $skip_ns= 0;
    foreach my $p ( $self->{NamespaceStack}->get_declared_prefixes() ) {
        $skip_ns= 1;
        my $uri = $self->{NamespaceStack}->get_uri($p);
        my $nodeflag = 0;
        if ( defined $uri
             and defined $el->{NamespaceURI}
             and $uri eq $el->{NamespaceURI} ) {
            #            $nodeflag = 1;
            next;
        }
        $node->setNamespace($uri, $p, 0 );
    }

    $self->{Parent} = $node;

    $self->{NamespaceStack}->push_context;

    # do attributes
    foreach my $key (keys %{$el->{Attributes}}) {
        my $attr = $el->{Attributes}->{$key};
        if (ref($attr)) {
            # catch broken name/value pairs
            next unless $attr->{Name} ;
            next if $self->{USENAMESPACESTACK}
                    and ( $attr->{Name} eq "xmlns"
                          or ( defined $attr->{Prefix}
                               and $attr->{Prefix} eq "xmlns" ) );


            if ( defined $attr->{Prefix}
                 and $attr->{Prefix} eq "xmlns" and $skip_ns == 0 ) {
                # ok, the generator does not set namespaces correctly!
                my $uri = $attr->{Value};
                $node->setNamespace($uri,
                                    $attr->{Localname},
                                    $uri eq $el->{NamespaceURI} ? 1 : 0 );
            }
            else {
                $node->setAttributeNS($attr->{NamespaceURI} || "",
                                      $attr->{Name}, $attr->{Value});
            }
        }
        else {
            $node->setAttribute($key => $attr);
        }
    }
    return ();
}

sub end_element {
    my ($self, $el) = @_;
    return unless $self->{Parent};

    $self->{NamespaceStack}->pop_context;
    $self->{Parent} = $self->{Parent}->parentNode();
    return ();
}

sub start_cdata {
    my $self = shift;
    $self->{IN_CDATA} = 1;
    return ();
}

sub end_cdata {
    my $self = shift;
    $self->{IN_CDATA} = 0;
    return ();
}

sub characters {
    my ($self, $chars) = @_;
    if ( not defined $self->{DOM} and not defined $self->{Parent} ) {
        $self->{Parent} = XML::LibXML::DocumentFragment->new();
        $self->{NamespaceStack} = XML::NamespaceSupport->new;
        $self->{NamespaceStack}->push_context;
    }
    return unless $self->{Parent};
    my $node;

    unless ( defined $chars and defined $chars->{Data} ) {
        return;
    }

    if ( defined $self->{DOM} ) {
        if ( defined $self->{IN_CDATA} and $self->{IN_CDATA} == 1 ) {
            $node = $self->{DOM}->createCDATASection($chars->{Data});
        }
        else {
            $node = $self->{Parent}->appendText($chars->{Data});
            return;
        }
    }
    elsif ( defined $self->{IN_CDATA} and $self->{IN_CDATA} == 1 ) {
        $node = XML::LibXML::CDATASection->new($chars->{Data});
    }
    else {
        $node = XML::LibXML::Text->new($chars->{Data});
    }

    $self->{Parent}->addChild($node);
    return ();
}

sub comment {
    my ($self, $chars) = @_;
    my $comment;
    if ( not defined $self->{DOM} and not defined $self->{Parent} ) {
        $self->{Parent} = XML::LibXML::DocumentFragment->new();
        $self->{NamespaceStack} = XML::NamespaceSupport->new;
        $self->{NamespaceStack}->push_context;
    }

    unless ( defined $chars and defined $chars->{Data} ) {
        return;
    }

    if ( defined $self->{DOM} ) {
        $comment = $self->{DOM}->createComment( $chars->{Data} );
    }
    else {
        $comment = XML::LibXML::Comment->new( $chars->{Data} );
    }

    if ( defined $self->{Parent} ) {
        $self->{Parent}->addChild($comment);
    }
    else {
        $self->{DOM}->addChild($comment);
    }
    return ();
}

sub processing_instruction {
    my ( $self,  $pi ) = @_;
    my $PI;
    return unless  defined $self->{DOM};
    $PI = $self->{DOM}->createPI( $pi->{Target}, $pi->{Data} );

    if ( defined $self->{Parent} ) {
        $self->{Parent}->addChild( $PI );
    }
    else {
        $self->{DOM}->addChild( $PI );
    }
    return ();
}

sub warning {
    my $self = shift;
    my $error = shift;
    # fill $@ but do not die seriously
    eval { $error->throw; };
}

sub error {
    my $self = shift;
    my $error = shift;
    delete $self->{NamespaceStack};
    delete $self->{Parent};
    delete $self->{DOM};
    $error->throw;
}

sub fatal_error {
    my $self = shift;
    my $error = shift;
    delete $self->{NamespaceStack};
    delete $self->{Parent};
    delete $self->{DOM};
    $error->throw;
}

1;

__END__
