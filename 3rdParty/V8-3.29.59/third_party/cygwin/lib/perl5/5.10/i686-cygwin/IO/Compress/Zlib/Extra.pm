package IO::Compress::Zlib::Extra;

require 5.004 ;

use strict ;
use warnings;
use bytes;

our ($VERSION, @ISA, @EXPORT_OK, %EXPORT_TAGS);

$VERSION = '2.011';

use IO::Compress::Gzip::Constants 2.011 ;

sub ExtraFieldError
{
    return $_[0];
    return "Error with ExtraField Parameter: $_[0]" ;
}

sub validateExtraFieldPair
{
    my $pair = shift ;
    my $strict = shift;
    my $gzipMode = shift ;

    return ExtraFieldError("Not an array ref")
        unless ref $pair &&  ref $pair eq 'ARRAY';

    return ExtraFieldError("SubField must have two parts")
        unless @$pair == 2 ;

    return ExtraFieldError("SubField ID is a reference")
        if ref $pair->[0] ;

    return ExtraFieldError("SubField Data is a reference")
        if ref $pair->[1] ;

    # ID is exactly two chars   
    return ExtraFieldError("SubField ID not two chars long")
        unless length $pair->[0] == GZIP_FEXTRA_SUBFIELD_ID_SIZE ;

    # Check that the 2nd byte of the ID isn't 0    
    return ExtraFieldError("SubField ID 2nd byte is 0x00")
        if $strict && $gzipMode && substr($pair->[0], 1, 1) eq "\x00" ;

    return ExtraFieldError("SubField Data too long")
        if length $pair->[1] > GZIP_FEXTRA_SUBFIELD_MAX_SIZE ;


    return undef ;
}

sub parseRawExtra
{
    my $data     = shift ;
    my $extraRef = shift;
    my $strict   = shift;
    my $gzipMode = shift ;

    #my $lax = shift ;

    #return undef
    #    if $lax ;

    my $XLEN = length $data ;

    return ExtraFieldError("Too Large")
        if $XLEN > GZIP_FEXTRA_MAX_SIZE;

    my $offset = 0 ;
    while ($offset < $XLEN) {

        return ExtraFieldError("Truncated in FEXTRA Body Section")
            if $offset + GZIP_FEXTRA_SUBFIELD_HEADER_SIZE  > $XLEN ;

        my $id = substr($data, $offset, GZIP_FEXTRA_SUBFIELD_ID_SIZE);    
        $offset += GZIP_FEXTRA_SUBFIELD_ID_SIZE;

        my $subLen =  unpack("v", substr($data, $offset,
                                            GZIP_FEXTRA_SUBFIELD_LEN_SIZE));
        $offset += GZIP_FEXTRA_SUBFIELD_LEN_SIZE ;

        return ExtraFieldError("Truncated in FEXTRA Body Section")
            if $offset + $subLen > $XLEN ;

        my $bad = validateExtraFieldPair( [$id, 
                                           substr($data, $offset, $subLen)], 
                                           $strict, $gzipMode );
        return $bad if $bad ;
        push @$extraRef, [$id => substr($data, $offset, $subLen)]
            if defined $extraRef;;

        $offset += $subLen ;
    }

        
    return undef ;
}


sub mkSubField
{
    my $id = shift ;
    my $data = shift ;

    return $id . pack("v", length $data) . $data ;
}

sub parseExtraField
{
    my $dataRef  = $_[0];
    my $strict   = $_[1];
    my $gzipMode = $_[2];
    #my $lax     = @_ == 2 ? $_[1] : 1;


    # ExtraField can be any of
    #
    #    -ExtraField => $data
    #
    #    -ExtraField => [$id1, $data1,
    #                    $id2, $data2]
    #                     ...
    #                   ]
    #
    #    -ExtraField => [ [$id1 => $data1],
    #                     [$id2 => $data2],
    #                     ...
    #                   ]
    #
    #    -ExtraField => { $id1 => $data1,
    #                     $id2 => $data2,
    #                     ...
    #                   }
    
    if ( ! ref $dataRef ) {

        return undef
            if ! $strict;

        return parseRawExtra($dataRef, undef, 1, $gzipMode);
    }

    #my $data = $$dataRef;
    my $data = $dataRef;
    my $out = '' ;

    if (ref $data eq 'ARRAY') {    
        if (ref $data->[0]) {

            foreach my $pair (@$data) {
                return ExtraFieldError("Not list of lists")
                    unless ref $pair eq 'ARRAY' ;

                my $bad = validateExtraFieldPair($pair, $strict, $gzipMode) ;
                return $bad if $bad ;

                $out .= mkSubField(@$pair);
            }   
        }   
        else {
            return ExtraFieldError("Not even number of elements")
                unless @$data % 2  == 0;

            for (my $ix = 0; $ix <= length(@$data) -1 ; $ix += 2) {
                my $bad = validateExtraFieldPair([$data->[$ix],
                                                  $data->[$ix+1]], 
                                                 $strict, $gzipMode) ;
                return $bad if $bad ;

                $out .= mkSubField($data->[$ix], $data->[$ix+1]);
            }   
        }
    }   
    elsif (ref $data eq 'HASH') {    
        while (my ($id, $info) = each %$data) {
            my $bad = validateExtraFieldPair([$id, $info], $strict, $gzipMode);
            return $bad if $bad ;

            $out .= mkSubField($id, $info);
        }   
    }   
    else {
        return ExtraFieldError("Not a scalar, array ref or hash ref") ;
    }

    return ExtraFieldError("Too Large")
        if length $out > GZIP_FEXTRA_MAX_SIZE;

    $_[0] = $out ;

    return undef;
}

1;

__END__
