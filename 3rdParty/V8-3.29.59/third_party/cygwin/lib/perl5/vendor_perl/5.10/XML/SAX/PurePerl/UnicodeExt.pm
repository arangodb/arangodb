# $Id: UnicodeExt.pm,v 1.1 2002/01/30 17:35:21 matt Exp $

package XML::SAX::PurePerl;
use strict;

no warnings 'utf8';

sub chr_ref {
    return chr(shift);
}

if ($] >= 5.007002) {
    require Encode;
    
    Encode::define_alias( "UTF-16" => "UCS-2" );
    Encode::define_alias( "UTF-16BE" => "UCS-2" );
    Encode::define_alias( "UTF-16LE" => "ucs-2le" );
    Encode::define_alias( "UTF16LE" => "ucs-2le" );
}

1;

