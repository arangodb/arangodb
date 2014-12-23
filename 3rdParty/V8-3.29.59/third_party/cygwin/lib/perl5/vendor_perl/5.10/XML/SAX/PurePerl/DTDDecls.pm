# $Id: DTDDecls.pm,v 1.7 2005/10/14 20:31:20 matt Exp $

package XML::SAX::PurePerl;

use strict;
use XML::SAX::PurePerl::Productions qw($NameChar $SingleChar);

sub elementdecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(9);
    return 0 unless $data =~ /^<!ELEMENT/;
    $reader->move_along(9);
    
    $self->skip_whitespace($reader) ||
        $self->parser_error("No whitespace after ELEMENT declaration", $reader);
    
    my $name = $self->Name($reader);
    
    $self->skip_whitespace($reader) ||
        $self->parser_error("No whitespace after ELEMENT's name", $reader);
        
    $self->contentspec($reader, $name);
    
    $self->skip_whitespace($reader);
    
    $reader->match('>') or $self->parser_error("Closing angle bracket not found on ELEMENT declaration", $reader);
    
    return 1;
}

sub contentspec {
    my ($self, $reader, $name) = @_;
    
    my $data = $reader->data(5);
    
    my $model;
    if ($data =~ /^EMPTY/) {
        $reader->move_along(5);
        $model = 'EMPTY';
    }
    elsif ($data =~ /^ANY/) {
        $reader->move_along(3);
        $model = 'ANY';
    }
    else {
        $model = $self->Mixed_or_children($reader);
    }

    if ($model) {
        # call SAX callback now.
        $self->element_decl({Name => $name, Model => $model});
        return 1;
    }
    
    $self->parser_error("contentspec not found in ELEMENT declaration", $reader);
}

sub Mixed_or_children {
    my ($self, $reader) = @_;

    my $data = $reader->data(8);
    $data =~ /^\(/ or return; # $self->parser_error("No opening bracket in Mixed or children", $reader);
    
    if ($data =~ /^\(\s*\#PCDATA/) {
        $reader->match('(');
        $self->skip_whitespace($reader);
        $reader->move_along(7);
        my $model = $self->Mixed($reader);
        return $model;
    }

    # not matched - must be Children
    return $self->children($reader);
}

# Mixed ::= ( '(' S* PCDATA ( S* '|' S* QName )* S* ')' '*' )
#               | ( '(' S* PCDATA S* ')' )
sub Mixed {
    my ($self, $reader) = @_;

    # Mixed_or_children already matched '(' S* '#PCDATA'

    my $model = '(#PCDATA';
            
    $self->skip_whitespace($reader);

    my %seen;
    
    while (1) {
        last unless $reader->match('|');
        $self->skip_whitespace($reader);

        my $name = $self->Name($reader) || 
            $self->parser_error("No 'Name' after Mixed content '|'", $reader);

        if ($seen{$name}) {
            $self->parser_error("Element '$name' has already appeared in this group", $reader);
        }
        $seen{$name}++;

        $model .= "|$name";
        
        $self->skip_whitespace($reader);
    }
    
    $reader->match(')') || $self->parser_error("no closing bracket on mixed content", $reader);

    $model .= ")";

    if ($reader->match('*')) {
        $model .= "*";
    }
    
    return $model;
}

# [[47]] Children ::= ChoiceOrSeq Cardinality?
# [[48]] Cp ::= ( QName | ChoiceOrSeq ) Cardinality?
#       ChoiceOrSeq ::= '(' S* Cp ( Choice | Seq )? S* ')'
# [[49]] Choice ::= ( S* '|' S* Cp )+
# [[50]] Seq    ::= ( S* ',' S* Cp )+
#        // Children ::= (Choice | Seq) Cardinality?
#        // Cp ::= ( QName | Choice | Seq) Cardinality?
#        // Choice ::= '(' S* Cp ( S* '|' S* Cp )+ S* ')'
#        // Seq    ::= '(' S* Cp ( S* ',' S* Cp )* S* ')'
# [[51]] Mixed ::= ( '(' S* PCDATA ( S* '|' S* QName )* S* ')' MixedCardinality )
#                | ( '(' S* PCDATA S* ')' )
#        Cardinality ::= '?' | '+' | '*'
#        MixedCardinality ::= '*'
sub children {
    my ($self, $reader) = @_;
    
    return $self->ChoiceOrSeq($reader) . $self->Cardinality($reader);
}

sub ChoiceOrSeq {
    my ($self, $reader) = @_;
    
    $reader->match('(') or $self->parser_error("choice/seq contains no opening bracket", $reader);
    
    my $model = '(';
    
    $self->skip_whitespace($reader);

    $model .= $self->Cp($reader);
    
    if (my $choice = $self->Choice($reader)) {
        $model .= $choice;
    }
    else {
        $model .= $self->Seq($reader);
    }

    $self->skip_whitespace($reader);

    $reader->match(')') or $self->parser_error("choice/seq contains no closing bracket", $reader);

    $model .= ')';
    
    return $model;
}

sub Cardinality {
    my ($self, $reader) = @_;
    # cardinality is always optional
    my $data = $reader->data;
    if ($data =~ /^([\?\+\*])/) {
        $reader->move_along(1);
        return $1;
    }
    return '';
}

sub Cp {
    my ($self, $reader) = @_;

    my $model;
    my $name = eval
    {
	if (my $name = $self->Name($reader)) {
	    return $name . $self->Cardinality($reader);
	}
    };
    return $name if defined $name;
    return $self->ChoiceOrSeq($reader) . $self->Cardinality($reader);
}

sub Choice {
    my ($self, $reader) = @_;
    
    my $model = '';
    $self->skip_whitespace($reader);
    
    while ($reader->match('|')) {
        $self->skip_whitespace($reader);
        $model .= '|';
        $model .= $self->Cp($reader);
        $self->skip_whitespace($reader);
    }

    return $model;
}

sub Seq {
    my ($self, $reader) = @_;
    
    my $model = '';
    $self->skip_whitespace($reader);
    
    while ($reader->match(',')) {
        $self->skip_whitespace($reader);
        my $cp = $self->Cp($reader);
        if ($cp) {
            $model .= ',';
            $model .= $cp;
        }
        $self->skip_whitespace($reader);
    }

    return $model;
}

sub AttlistDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(9);
    if ($data =~ /^<!ATTLIST/) {
        # It's an attlist
        
        $reader->move_along(9);
        
        $self->skip_whitespace($reader) || 
            $self->parser_error("No whitespace after ATTLIST declaration", $reader);
        my $name = $self->Name($reader);

        $self->AttDefList($reader, $name);

        $self->skip_whitespace($reader);
        
        $reader->match('>') or $self->parser_error("Closing angle bracket not found on ATTLIST declaration", $reader);
        
        return 1;
    }
    
    return 0;
}

sub AttDefList {
    my ($self, $reader, $name) = @_;

    1 while $self->AttDef($reader, $name);
}

sub AttDef {
    my ($self, $reader, $el_name) = @_;

    $self->skip_whitespace($reader) || return 0;
    my $att_name = $self->Name($reader) || return 0;
    $self->skip_whitespace($reader) || 
        $self->parser_error("No whitespace after Name in attribute definition", $reader);
    my $att_type = $self->AttType($reader);

    $self->skip_whitespace($reader) ||
        $self->parser_error("No whitespace after AttType in attribute definition", $reader);
    my ($mode, $value) = $self->DefaultDecl($reader);
    
    # fire SAX event here!
    $self->attribute_decl({
            eName => $el_name, 
            aName => $att_name, 
            Type => $att_type, 
            Mode => $mode, 
            Value => $value,
            });
    return 1;
}

sub AttType {
    my ($self, $reader) = @_;

    return $self->StringType($reader) ||
            $self->TokenizedType($reader) ||
            $self->EnumeratedType($reader) ||
            $self->parser_error("Can't match AttType", $reader);
}

sub StringType {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(5);
    return unless $data =~ /^CDATA/;
    $reader->move_along(5);
    return 'CDATA';
}

sub TokenizedType {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(8);
    if ($data =~ /^(IDREFS?|ID|ENTITIES|ENTITY|NMTOKENS?)/) {
        $reader->move_along(length($1));
        return $1;
    }
    return;
}

sub EnumeratedType {
    my ($self, $reader) = @_;
    return $self->NotationType($reader) || $self->Enumeration($reader);
}

sub NotationType {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(8);
    return unless $data =~ /^NOTATION/;
    $reader->move_along(8);
    
    $self->skip_whitespace($reader) ||
        $self->parser_error("No whitespace after NOTATION", $reader);
    $reader->match('(') or $self->parser_error("No opening bracket in notation section", $reader);
    
    $self->skip_whitespace($reader);
    my $model = 'NOTATION (';
    my $name = $self->Name($reader) ||
        $self->parser_error("No name in notation section", $reader);
    $model .= $name;
    $self->skip_whitespace($reader);
    $data = $reader->data;
    while ($data =~ /^\|/) {
        $reader->move_along(1);
        $model .= '|';
        $self->skip_whitespace($reader);
        my $name = $self->Name($reader) ||
            $self->parser_error("No name in notation section", $reader);
        $model .= $name;
        $self->skip_whitespace($reader);
        $data = $reader->data;
    }
    $data =~ /^\)/ or $self->parser_error("No closing bracket in notation section", $reader);
    $reader->move_along(1);
    
    $model .= ')';

    return $model;
}

sub Enumeration {
    my ($self, $reader) = @_;
    
    return unless $reader->match('(');
    
    $self->skip_whitespace($reader);
    my $model = '(';
    my $nmtoken = $self->Nmtoken($reader) ||
        $self->parser_error("No Nmtoken in enumerated declaration", $reader);
    $model .= $nmtoken;
    $self->skip_whitespace($reader);
    my $data = $reader->data;
    while ($data =~ /^\|/) {
        $model .= '|';
        $reader->move_along(1);
        $self->skip_whitespace($reader);
        my $nmtoken = $self->Nmtoken($reader) ||
            $self->parser_error("No Nmtoken in enumerated declaration", $reader);
        $model .= $nmtoken;
        $self->skip_whitespace($reader);
        $data = $reader->data;
    }
    $data =~ /^\)/ or $self->parser_error("No closing bracket in enumerated declaration", $reader);
    $reader->move_along(1);
    
    $model .= ')';

    return $model;
}

sub Nmtoken {
    my ($self, $reader) = @_;
    return $self->Name($reader);
}

sub DefaultDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(9);
    if ($data =~ /^(\#REQUIRED|\#IMPLIED)/) {
        $reader->move_along(length($1));
        return $1;
    }
    my $model = '';
    if ($data =~ /^\#FIXED/) {
        $reader->move_along(6);
        $self->skip_whitespace($reader) || $self->parser_error(
                "no whitespace after FIXED specifier", $reader);
        my $value = $self->AttValue($reader);
        return "#FIXED", $value;
    }
    my $value = $self->AttValue($reader);
    return undef, $value;
}

sub EntityDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(8);
    return 0 unless $data =~ /^<!ENTITY/;
    $reader->move_along(8);
    
    $self->skip_whitespace($reader) || $self->parser_error(
        "No whitespace after ENTITY declaration", $reader);
    
    $self->PEDecl($reader) || $self->GEDecl($reader);
    
    $self->skip_whitespace($reader);
    
    $reader->match('>') or $self->parser_error("No closing '>' in entity definition", $reader);
    
    return 1;
}

sub GEDecl {
    my ($self, $reader) = @_;

    my $name = $self->Name($reader) || $self->parser_error("No entity name given", $reader);
    $self->skip_whitespace($reader) || $self->parser_error("No whitespace after entity name", $reader);

    # TODO: ExternalID calls lexhandler method. Wrong place for it.
    my $value;
    if ($value = $self->ExternalID($reader)) {
        $value .= $self->NDataDecl($reader);
    }
    else {
        $value = $self->EntityValue($reader);
    }

    if ($self->{ParseOptions}{entities}{$name}) {
        warn("entity $name already exists\n");
    } else {
        $self->{ParseOptions}{entities}{$name} = 1;
        $self->{ParseOptions}{expanded_entity}{$name} = $value; # ???
    }
    # do callback?
    return 1;
}

sub PEDecl {
    my ($self, $reader) = @_;
    
    return 0 unless $reader->match('%');

    $self->skip_whitespace($reader) || $self->parser_error("No whitespace after parameter entity marker", $reader);
    my $name = $self->Name($reader) || $self->parser_error("No parameter entity name given", $reader);
    $self->skip_whitespace($reader) || $self->parser_error("No whitespace after parameter entity name", $reader);
    my $value = $self->ExternalID($reader) ||
                $self->EntityValue($reader) ||
                $self->parser_error("PE is not a value or an external resource", $reader);
    # do callback?
    return 1;
}

my $quotre = qr/[^%&\"]/;
my $aposre = qr/[^%&\']/;

sub EntityValue {
    my ($self, $reader) = @_;
    
    my $data = $reader->data;
    my $quote = '"';
    my $re = $quotre;
    if (!$data =~ /^"/) {
        $data =~ /^'/ or $self->parser_error("Not a quote character", $reader);
        $quote = "'";
        $re = $aposre;
    }
    $reader->move_along(1);
    
    my $value = '';
    
    while (1) {
        my $data = $reader->data;

        $self->parser_error("EOF found while reading entity value", $reader)
            unless length($data);
        
        if ($data =~ /^($re+)/) {
            my $match = $1;
            $value .= $match;
            $reader->move_along(length($match));
        }
        elsif ($reader->match('&')) {
            # if it's a char ref, expand now:
            if ($reader->match('#')) {
                my $char;
                my $ref = '';
                if ($reader->match('x')) {
                    my $data = $reader->data;
                    while (1) {
                        $self->parser_error("EOF looking for reference end", $reader)
                            unless length($data);
                        if ($data !~ /^([0-9a-fA-F]*)/) {
                            last;
                        }
                        $ref .= $1;
                        $reader->move_along(length($1));
                        if (length($1) == length($data)) {
                            $data = $reader->data;
                        }
                        else {
                            last;
                        }
                    }
                    $char = chr_ref(hex($ref));
                    $ref = "x$ref";
                }
                else {
                    my $data = $reader->data;
                    while (1) {
                        $self->parser_error("EOF looking for reference end", $reader)
                            unless length($data);
                        if ($data !~ /^([0-9]*)/) {
                            last;
                        }
                        $ref .= $1;
                        $reader->move_along(length($1));
                        if (length($1) == length($data)) {
                            $data = $reader->data;
                        }
                        else {
                            last;
                        }
                    }
                    $char = chr($ref);
                }
                $reader->match(';') ||
                    $self->parser_error("No semi-colon found after character reference", $reader);
                if ($char !~ $SingleChar) { # match a single character
                    $self->parser_error("Character reference '&#$ref;' refers to an illegal XML character ($char)", $reader);
                }
                $value .= $char;
            }
            else {
                # entity refs in entities get expanded later, so don't parse now.
                $value .= '&';
            }
        }
        elsif ($reader->match('%')) {
            $value .= $self->PEReference($reader);
        }
        elsif ($reader->match($quote)) {
            # end of attrib
            last;
        }
        else {
            $self->parser_error("Invalid character in attribute value: " . substr($reader->data, 0, 1), $reader);
        }
    }
    
    return $value;
}

sub NDataDecl {
    my ($self, $reader) = @_;
    $self->skip_whitespace($reader) || return '';
    my $data = $reader->data(5);
    return '' unless $data =~ /^NDATA/;
    $reader->move_along(5);
    $self->skip_whitespace($reader) || $self->parser_error("No whitespace after NDATA declaration", $reader);
    my $name = $self->Name($reader) || $self->parser_error("NDATA declaration lacks a proper Name", $reader);
    return " NDATA $name";
}

sub NotationDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(10);
    return 0 unless $data =~ /^<!NOTATION/;
    $reader->move_along(10);
    $self->skip_whitespace($reader) ||
        $self->parser_error("No whitespace after NOTATION declaration", $reader);
    $data = $reader->data;
    my $value = '';
    while(1) {
        $self->parser_error("EOF found while looking for end of NotationDecl", $reader)
            unless length($data);
        
        if ($data =~ /^([^>]*)>/) {
            $value .= $1;
            $reader->move_along(length($1) + 1);
            $self->notation_decl({Name => "FIXME", SystemId => "FIXME", PublicId => "FIXME" });
            last;
        }
        else {
            $value .= $data;
            $reader->move_along(length($data));
            $data = $reader->data;
        }
    }
    return 1;
}

1;
