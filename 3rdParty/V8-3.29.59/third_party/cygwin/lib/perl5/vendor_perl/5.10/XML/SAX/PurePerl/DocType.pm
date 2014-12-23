# $Id: DocType.pm,v 1.3 2003/07/30 13:39:22 matt Exp $

package XML::SAX::PurePerl;

use strict;
use XML::SAX::PurePerl::Productions qw($PubidChar);

sub doctypedecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(9);
    if ($data =~ /^<!DOCTYPE/) {
        $reader->move_along(9);
        $self->skip_whitespace($reader) ||
            $self->parser_error("No whitespace after doctype declaration", $reader);
        
        my $root_name = $self->Name($reader) ||
            $self->parser_error("Doctype declaration has no root element name", $reader);
        
        if ($self->skip_whitespace($reader)) {
            # might be externalid...
            my %dtd = $self->ExternalID($reader);
            # TODO: Call SAX event
        }
        
        $self->skip_whitespace($reader);
        
        $self->InternalSubset($reader);
        
        $reader->match('>') or $self->parser_error("Doctype not closed", $reader);
        
        return 1;
    }
    
    return 0;
}

sub ExternalID {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(6);
    
    if ($data =~ /^SYSTEM/) {
        $reader->move_along(6);
        $self->skip_whitespace($reader) ||
            $self->parser_error("No whitespace after SYSTEM identifier", $reader);
        return (SYSTEM => $self->SystemLiteral($reader));
    }
    elsif ($data =~ /^PUBLIC/) {
        $reader->move_along(6);
        $self->skip_whitespace($reader) ||
            $self->parser_error("No whitespace after PUBLIC identifier", $reader);
        
        my $quote = $self->quote($reader) || 
            $self->parser_error("Not a quote character in PUBLIC identifier", $reader);
        
        my $data = $reader->data;
        my $pubid = '';
        while(1) {
            $self->parser_error("EOF while looking for end of PUBLIC identifiier", $reader)
                unless length($data);
            
            if ($data =~ /^([^$quote]*)$quote/) {
                $pubid .= $1;
                $reader->move_along(length($1) + 1);
                last;
            }
            else {
                $pubid .= $data;
                $reader->move_along(length($data));
                $data = $reader->data;
            }
        }
        
        if ($pubid !~ /^($PubidChar)+$/) {
            $self->parser_error("Invalid characters in PUBLIC identifier", $reader);
        }
        
        $self->skip_whitespace($reader) ||
            $self->parser_error("Not whitespace after PUBLIC ID in DOCTYPE", $reader);
        
        return (PUBLIC => $pubid, 
                SYSTEM => $self->SystemLiteral($reader));
    }
    else {
        return;
    }
    
    return 1;
}

sub SystemLiteral {
    my ($self, $reader) = @_;
    
    my $quote = $self->quote($reader);
    
    my $data = $reader->data;
    my $systemid = '';
    while (1) {
        $self->parser_error("EOF found while looking for end of Sytem Literal", $reader)
            unless length($data);
        if ($data =~ /^([^$quote]*)$quote/) {
            $systemid .= $1;
            $reader->move_along(length($1) + 1);
            return $systemid;
        }
        else {
            $systemid .= $data;
            $reader->move_along(length($data));
            $data = $reader->data;
        }
    }
}

sub InternalSubset {
    my ($self, $reader) = @_;
    
    return 0 unless $reader->match('[');
    
    1 while $self->IntSubsetDecl($reader);
    
    $reader->match(']') or $self->parser_error("No close bracket on internal subset (found: " . $reader->data, $reader);
    $self->skip_whitespace($reader);
    return 1;
}

sub IntSubsetDecl {
    my ($self, $reader) = @_;

    return $self->DeclSep($reader) || $self->markupdecl($reader);
}

sub DeclSep {
    my ($self, $reader) = @_;

    if ($self->skip_whitespace($reader)) {
        return 1;
    }

    if ($self->PEReference($reader)) {
        return 1;
    }
    
#    if ($self->ParsedExtSubset($reader)) {
#        return 1;
#    }
    
    return 0;
}

sub PEReference {
    my ($self, $reader) = @_;
    
    return 0 unless $reader->match('%');
    
    my $peref = $self->Name($reader) ||
        $self->parser_error("PEReference did not find a Name", $reader);
    # TODO - load/parse the peref
    
    $reader->match(';') or $self->parser_error("Invalid token in PEReference", $reader);
    return 1;
}

sub markupdecl {
    my ($self, $reader) = @_;
    
    if ($self->elementdecl($reader) ||
        $self->AttlistDecl($reader) ||
        $self->EntityDecl($reader) ||
        $self->NotationDecl($reader) ||
        $self->PI($reader) ||
        $self->Comment($reader))
    {
        return 1;
    }
    
    return 0;
}

1;
