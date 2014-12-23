# $Id: XMLDecl.pm,v 1.3 2003/07/30 13:39:22 matt Exp $

package XML::SAX::PurePerl;

use strict;
use XML::SAX::PurePerl::Productions qw($S $VersionNum $EncNameStart $EncNameEnd);

sub XMLDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(5);
    # warn("Looking for xmldecl in: $data");
    if ($data =~ /^<\?xml$S/o) {
        $reader->move_along(5);
        $self->skip_whitespace($reader);
        
        # get version attribute
        $self->VersionInfo($reader) || 
            $self->parser_error("XML Declaration lacks required version attribute, or version attribute does not match XML specification", $reader);
        
        if (!$self->skip_whitespace($reader)) {
            my $data = $reader->data(2);
            $data =~ /^\?>/ or $self->parser_error("Syntax error", $reader);
            $reader->move_along(2);
            return;
        }
        
        if ($self->EncodingDecl($reader)) {
            if (!$self->skip_whitespace($reader)) {
                my $data = $reader->data(2);
                $data =~ /^\?>/ or $self->parser_error("Syntax error", $reader);
                $reader->move_along(2);
                return;
            }
        }
        
        $self->SDDecl($reader);
        
        $self->skip_whitespace($reader);
        
        my $data = $reader->data(2);
        $data =~ /^\?>/ or $self->parser_error("Syntax error", $reader);
        $reader->move_along(2);
    }
    else {
        # warn("first 5 bytes: ", join(',', unpack("CCCCC", $data)), "\n");
        # no xml decl
        if (!$reader->get_encoding) {
            $reader->set_encoding("UTF-8");
        }
    }
}

sub VersionInfo {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(11);
    
    # warn("Looking for version in $data");
    
    $data =~ /^(version$S*=$S*(["'])($VersionNum)\2)/o or return 0;
    $reader->move_along(length($1));
    my $vernum = $3;
    
    if ($vernum ne "1.0") {
        $self->parser_error("Only XML version 1.0 supported. Saw: '$vernum'", $reader);
    }

    return 1;
}

sub SDDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(15);
    
    $data =~ /^(standalone$S*=$S*(["'])(yes|no)\2)/o or return 0;
    $reader->move_along(length($1));
    my $yesno = $3;
    
    if ($yesno eq 'yes') {
        $self->{standalone} = 1;
    }
    else {
        $self->{standalone} = 0;
    }
    
    return 1;
}

sub EncodingDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(12);
    
    $data =~ /^(encoding$S*=$S*(["'])($EncNameStart$EncNameEnd*)\2)/o or return 0;
    $reader->move_along(length($1));
    my $encoding = $3;
    
    $reader->set_encoding($encoding);
    
    return 1;
}

sub TextDecl {
    my ($self, $reader) = @_;
    
    my $data = $reader->data(6);
    $data =~ /^<\?xml$S+/ or return;
    $reader->move_along(5);
    $self->skip_whitespace($reader);
    
    if ($self->VersionInfo($reader)) {
        $self->skip_whitespace($reader) ||
                $self->parser_error("Lack of whitespace after version attribute in text declaration", $reader);
    }
    
    $self->EncodingDecl($reader) ||
        $self->parser_error("Encoding declaration missing from external entity text declaration", $reader);
    
    $self->skip_whitespace($reader);
    
    $data = $reader->data(2);
    $data =~ /^\?>/ or $self->parser_error("Syntax error", $reader);
    
    return 1;
}

1;
