# $Id: EncodingDetect.pm,v 1.6 2007/02/07 09:33:50 grant Exp $

package XML::SAX::PurePerl; # NB, not ::EncodingDetect!

use strict;

sub encoding_detect {
    my ($parser, $reader) = @_;
    
    my $error = "Invalid byte sequence at start of file";
    
    my $data = $reader->data;
    if ($data =~ /^\x00\x00\xFE\xFF/) {
        # BO-UCS4-be
        $reader->move_along(4);
        $reader->set_encoding('UCS-4BE');
        return;
    }
    elsif ($data =~ /^\x00\x00\xFF\xFE/) {
        # BO-UCS-4-2143
        $reader->move_along(4);
        $reader->set_encoding('UCS-4-2143');
        return;
    }
    elsif ($data =~ /^\x00\x00\x00\x3C/) {
        $reader->set_encoding('UCS-4BE');
        return;
    }
    elsif ($data =~ /^\x00\x00\x3C\x00/) {
        $reader->set_encoding('UCS-4-2143');
        return;
    }
    elsif ($data =~ /^\x00\x3C\x00\x00/) {
        $reader->set_encoding('UCS-4-3412');
        return;
    }
    elsif ($data =~ /^\x00\x3C\x00\x3F/) {
        $reader->set_encoding('UTF-16BE');
        return;
    }
    elsif ($data =~ /^\xFF\xFE\x00\x00/) {
        # BO-UCS-4LE
        $reader->move_along(4);
        $reader->set_encoding('UCS-4LE');
        return;
    }
    elsif ($data =~ /^\xFF\xFE/) {
        $reader->move_along(2);
        $reader->set_encoding('UTF-16LE');
        return;
    }
    elsif ($data =~ /^\xFE\xFF\x00\x00/) {
        $reader->move_along(4);
        $reader->set_encoding('UCS-4-3412');
        return;
    }
    elsif ($data =~ /^\xFE\xFF/) {
        $reader->move_along(2);
        $reader->set_encoding('UTF-16BE');
        return;
    }
    elsif ($data =~ /^\xEF\xBB\xBF/) { # UTF-8 BOM
        $reader->move_along(3);
        $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^\x3C\x00\x00\x00/) {
        $reader->set_encoding('UCS-4LE');
        return;
    }
    elsif ($data =~ /^\x3C\x00\x3F\x00/) {
        $reader->set_encoding('UTF-16LE');
        return;
    }
    elsif ($data =~ /^\x3C\x3F\x78\x6D/) {
        # $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^\x3C\x3F\x78/) {
        # $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^\x3C\x3F/) {
        # $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^\x3C/) {
        # $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^[\x20\x09\x0A\x0D]+\x3C[^\x3F]/) {
        # $reader->set_encoding('UTF-8');
        return;
    }
    elsif ($data =~ /^\x4C\x6F\xA7\x94/) {
        $reader->set_encoding('EBCDIC');
        return;
    }
    
    warn("Unable to recognise encoding of this document");
    return;
}

1;

