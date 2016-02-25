
package Compress::Raw::Zlib;

require 5.004 ;
require Exporter;
use AutoLoader;
use Carp ;

#use Parse::Parameters;

use strict ;
use warnings ;
use bytes ;
our ($VERSION, $XS_VERSION, @ISA, @EXPORT, $AUTOLOAD);

$VERSION = '2.011';
$XS_VERSION = $VERSION; 
$VERSION = eval $VERSION;

@ISA = qw(Exporter);
# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.
@EXPORT = qw(
        adler32 crc32

        ZLIB_VERSION
        ZLIB_VERNUM

        DEF_WBITS
        OS_CODE

        MAX_MEM_LEVEL
        MAX_WBITS

        Z_ASCII
        Z_BEST_COMPRESSION
        Z_BEST_SPEED
        Z_BINARY
        Z_BLOCK
        Z_BUF_ERROR
        Z_DATA_ERROR
        Z_DEFAULT_COMPRESSION
        Z_DEFAULT_STRATEGY
        Z_DEFLATED
        Z_ERRNO
        Z_FILTERED
        Z_FIXED
        Z_FINISH
        Z_FULL_FLUSH
        Z_HUFFMAN_ONLY
        Z_MEM_ERROR
        Z_NEED_DICT
        Z_NO_COMPRESSION
        Z_NO_FLUSH
        Z_NULL
        Z_OK
        Z_PARTIAL_FLUSH
        Z_RLE
        Z_STREAM_END
        Z_STREAM_ERROR
        Z_SYNC_FLUSH
        Z_UNKNOWN
        Z_VERSION_ERROR
);


sub AUTOLOAD {
    my($constname);
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my ($error, $val) = constant($constname);
    Carp::croak $error if $error;
    no strict 'refs';
    *{$AUTOLOAD} = sub { $val };
    goto &{$AUTOLOAD};
}

use constant FLAG_APPEND             => 1 ;
use constant FLAG_CRC                => 2 ;
use constant FLAG_ADLER              => 4 ;
use constant FLAG_CONSUME_INPUT      => 8 ;

eval {
    require XSLoader;
    XSLoader::load('Compress::Raw::Zlib', $XS_VERSION);
    1;
} 
or do {
    require DynaLoader;
    local @ISA = qw(DynaLoader);
    bootstrap Compress::Raw::Zlib $XS_VERSION ; 
};
 

use constant Parse_any      => 0x01;
use constant Parse_unsigned => 0x02;
use constant Parse_signed   => 0x04;
use constant Parse_boolean  => 0x08;
use constant Parse_string   => 0x10;
use constant Parse_custom   => 0x12;

use constant Parse_store_ref => 0x100 ;

use constant OFF_PARSED     => 0 ;
use constant OFF_TYPE       => 1 ;
use constant OFF_DEFAULT    => 2 ;
use constant OFF_FIXED      => 3 ;
use constant OFF_FIRST_ONLY => 4 ;
use constant OFF_STICKY     => 5 ;



sub ParseParameters
{
    my $level = shift || 0 ; 

    my $sub = (caller($level + 1))[3] ;
    #local $Carp::CarpLevel = 1 ;
    my $p = new Compress::Raw::Zlib::Parameters() ;
    $p->parse(@_)
        or croak "$sub: $p->{Error}" ;

    return $p;
}


sub Compress::Raw::Zlib::Parameters::new
{
    my $class = shift ;

    my $obj = { Error => '',
                Got   => {},
              } ;

    #return bless $obj, ref($class) || $class || __PACKAGE__ ;
    return bless $obj, 'Compress::Raw::Zlib::Parameters' ;
}

sub Compress::Raw::Zlib::Parameters::setError
{
    my $self = shift ;
    my $error = shift ;
    my $retval = @_ ? shift : undef ;

    $self->{Error} = $error ;
    return $retval;
}
          
#sub getError
#{
#    my $self = shift ;
#    return $self->{Error} ;
#}
          
sub Compress::Raw::Zlib::Parameters::parse
{
    my $self = shift ;

    my $default = shift ;

    my $got = $self->{Got} ;
    my $firstTime = keys %{ $got } == 0 ;

    my (@Bad) ;
    my @entered = () ;

    # Allow the options to be passed as a hash reference or
    # as the complete hash.
    if (@_ == 0) {
        @entered = () ;
    }
    elsif (@_ == 1) {
        my $href = $_[0] ;    
        return $self->setError("Expected even number of parameters, got 1")
            if ! defined $href or ! ref $href or ref $href ne "HASH" ;
 
        foreach my $key (keys %$href) {
            push @entered, $key ;
            push @entered, \$href->{$key} ;
        }
    }
    else {
        my $count = @_;
        return $self->setError("Expected even number of parameters, got $count")
            if $count % 2 != 0 ;
        
        for my $i (0.. $count / 2 - 1) {
            push @entered, $_[2* $i] ;
            push @entered, \$_[2* $i+1] ;
        }
    }


    while (my ($key, $v) = each %$default)
    {
        croak "need 4 params [@$v]"
            if @$v != 4 ;

        my ($first_only, $sticky, $type, $value) = @$v ;
        my $x ;
        $self->_checkType($key, \$value, $type, 0, \$x) 
            or return undef ;

        $key = lc $key;

        if ($firstTime || ! $sticky) {
            $got->{$key} = [0, $type, $value, $x, $first_only, $sticky] ;
        }

        $got->{$key}[OFF_PARSED] = 0 ;
    }

    for my $i (0.. @entered / 2 - 1) {
        my $key = $entered[2* $i] ;
        my $value = $entered[2* $i+1] ;

        #print "Key [$key] Value [$value]" ;
        #print defined $$value ? "[$$value]\n" : "[undef]\n";

        $key =~ s/^-// ;
        my $canonkey = lc $key;
 
        if ($got->{$canonkey} && ($firstTime ||
                                  ! $got->{$canonkey}[OFF_FIRST_ONLY]  ))
        {
            my $type = $got->{$canonkey}[OFF_TYPE] ;
            my $s ;
            $self->_checkType($key, $value, $type, 1, \$s)
                or return undef ;
            #$value = $$value unless $type & Parse_store_ref ;
            $value = $$value ;
            $got->{$canonkey} = [1, $type, $value, $s] ;
        }
        else
          { push (@Bad, $key) }
    }
 
    if (@Bad) {
        my ($bad) = join(", ", @Bad) ;
        return $self->setError("unknown key value(s) @Bad") ;
    }

    return 1;
}

sub Compress::Raw::Zlib::Parameters::_checkType
{
    my $self = shift ;

    my $key   = shift ;
    my $value = shift ;
    my $type  = shift ;
    my $validate  = shift ;
    my $output  = shift;

    #local $Carp::CarpLevel = $level ;
    #print "PARSE $type $key $value $validate $sub\n" ;
    if ( $type & Parse_store_ref)
    {
        #$value = $$value
        #    if ref ${ $value } ;

        $$output = $value ;
        return 1;
    }

    $value = $$value ;

    if ($type & Parse_any)
    {
        $$output = $value ;
        return 1;
    }
    elsif ($type & Parse_unsigned)
    {
        return $self->setError("Parameter '$key' must be an unsigned int, got 'undef'")
            if $validate && ! defined $value ;
        return $self->setError("Parameter '$key' must be an unsigned int, got '$value'")
            if $validate && $value !~ /^\d+$/;

        $$output = defined $value ? $value : 0 ;    
        return 1;
    }
    elsif ($type & Parse_signed)
    {
        return $self->setError("Parameter '$key' must be a signed int, got 'undef'")
            if $validate && ! defined $value ;
        return $self->setError("Parameter '$key' must be a signed int, got '$value'")
            if $validate && $value !~ /^-?\d+$/;

        $$output = defined $value ? $value : 0 ;    
        return 1 ;
    }
    elsif ($type & Parse_boolean)
    {
        return $self->setError("Parameter '$key' must be an int, got '$value'")
            if $validate && defined $value && $value !~ /^\d*$/;
        $$output =  defined $value ? $value != 0 : 0 ;    
        return 1;
    }
    elsif ($type & Parse_string)
    {
        $$output = defined $value ? $value : "" ;    
        return 1;
    }

    $$output = $value ;
    return 1;
}



sub Compress::Raw::Zlib::Parameters::parsed
{
    my $self = shift ;
    my $name = shift ;

    return $self->{Got}{lc $name}[OFF_PARSED] ;
}

sub Compress::Raw::Zlib::Parameters::value
{
    my $self = shift ;
    my $name = shift ;

    if (@_)
    {
        $self->{Got}{lc $name}[OFF_PARSED]  = 1;
        $self->{Got}{lc $name}[OFF_DEFAULT] = $_[0] ;
        $self->{Got}{lc $name}[OFF_FIXED]   = $_[0] ;
    }

    return $self->{Got}{lc $name}[OFF_FIXED] ;
}

sub Compress::Raw::Zlib::Deflate::new
{
    my $pkg = shift ;
    my ($got) = ParseParameters(0,
            {
                'AppendOutput'  => [1, 1, Parse_boolean,  0],
                'CRC32'         => [1, 1, Parse_boolean,  0],
                'ADLER32'       => [1, 1, Parse_boolean,  0],
                'Bufsize'       => [1, 1, Parse_unsigned, 4096],
 
                'Level'         => [1, 1, Parse_signed,   Z_DEFAULT_COMPRESSION()],
                'Method'        => [1, 1, Parse_unsigned, Z_DEFLATED()],
                'WindowBits'    => [1, 1, Parse_signed,   MAX_WBITS()],
                'MemLevel'      => [1, 1, Parse_unsigned, MAX_MEM_LEVEL()],
                'Strategy'      => [1, 1, Parse_unsigned, Z_DEFAULT_STRATEGY()],
                'Dictionary'    => [1, 1, Parse_any,      ""],
            }, @_) ;


    croak "Compress::Raw::Zlib::Deflate::new: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $flags = 0 ;
    $flags |= FLAG_APPEND if $got->value('AppendOutput') ;
    $flags |= FLAG_CRC    if $got->value('CRC32') ;
    $flags |= FLAG_ADLER  if $got->value('ADLER32') ;

    _deflateInit($flags,
                $got->value('Level'), 
                $got->value('Method'), 
                $got->value('WindowBits'), 
                $got->value('MemLevel'), 
                $got->value('Strategy'), 
                $got->value('Bufsize'),
                $got->value('Dictionary')) ;

}

sub Compress::Raw::Zlib::Inflate::new
{
    my $pkg = shift ;
    my ($got) = ParseParameters(0,
                    {
                        'AppendOutput'  => [1, 1, Parse_boolean,  0],
                        'CRC32'         => [1, 1, Parse_boolean,  0],
                        'ADLER32'       => [1, 1, Parse_boolean,  0],
                        'ConsumeInput'  => [1, 1, Parse_boolean,  1],
                        'Bufsize'       => [1, 1, Parse_unsigned, 4096],
                 
                        'WindowBits'    => [1, 1, Parse_signed,   MAX_WBITS()],
                        'Dictionary'    => [1, 1, Parse_any,      ""],
            }, @_) ;


    croak "Compress::Raw::Zlib::Inflate::new: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $flags = 0 ;
    $flags |= FLAG_APPEND if $got->value('AppendOutput') ;
    $flags |= FLAG_CRC    if $got->value('CRC32') ;
    $flags |= FLAG_ADLER  if $got->value('ADLER32') ;
    $flags |= FLAG_CONSUME_INPUT if $got->value('ConsumeInput') ;

    _inflateInit($flags, $got->value('WindowBits'), $got->value('Bufsize'), 
                 $got->value('Dictionary')) ;
}

sub Compress::Raw::Zlib::InflateScan::new
{
    my $pkg = shift ;
    my ($got) = ParseParameters(0,
                    {
                        'CRC32'         => [1, 1, Parse_boolean,  0],
                        'ADLER32'       => [1, 1, Parse_boolean,  0],
                        'Bufsize'       => [1, 1, Parse_unsigned, 4096],
                 
                        'WindowBits'    => [1, 1, Parse_signed,   -MAX_WBITS()],
                        'Dictionary'    => [1, 1, Parse_any,      ""],
            }, @_) ;


    croak "Compress::Raw::Zlib::InflateScan::new: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $flags = 0 ;
    #$flags |= FLAG_APPEND if $got->value('AppendOutput') ;
    $flags |= FLAG_CRC    if $got->value('CRC32') ;
    $flags |= FLAG_ADLER  if $got->value('ADLER32') ;
    #$flags |= FLAG_CONSUME_INPUT if $got->value('ConsumeInput') ;

    _inflateScanInit($flags, $got->value('WindowBits'), $got->value('Bufsize'), 
                 '') ;
}

sub Compress::Raw::Zlib::inflateScanStream::createDeflateStream
{
    my $pkg = shift ;
    my ($got) = ParseParameters(0,
            {
                'AppendOutput'  => [1, 1, Parse_boolean,  0],
                'CRC32'         => [1, 1, Parse_boolean,  0],
                'ADLER32'       => [1, 1, Parse_boolean,  0],
                'Bufsize'       => [1, 1, Parse_unsigned, 4096],
 
                'Level'         => [1, 1, Parse_signed,   Z_DEFAULT_COMPRESSION()],
                'Method'        => [1, 1, Parse_unsigned, Z_DEFLATED()],
                'WindowBits'    => [1, 1, Parse_signed,   - MAX_WBITS()],
                'MemLevel'      => [1, 1, Parse_unsigned, MAX_MEM_LEVEL()],
                'Strategy'      => [1, 1, Parse_unsigned, Z_DEFAULT_STRATEGY()],
            }, @_) ;

    croak "Compress::Raw::Zlib::InflateScan::createDeflateStream: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        unless $got->value('Bufsize') >= 1;

    my $flags = 0 ;
    $flags |= FLAG_APPEND if $got->value('AppendOutput') ;
    $flags |= FLAG_CRC    if $got->value('CRC32') ;
    $flags |= FLAG_ADLER  if $got->value('ADLER32') ;

    $pkg->_createDeflateStream($flags,
                $got->value('Level'), 
                $got->value('Method'), 
                $got->value('WindowBits'), 
                $got->value('MemLevel'), 
                $got->value('Strategy'), 
                $got->value('Bufsize'),
                ) ;

}

sub Compress::Raw::Zlib::inflateScanStream::inflate
{
    my $self = shift ;
    my $buffer = $_[1];
    my $eof = $_[2];

    my $status = $self->scan(@_);

    if ($status == Z_OK() && $_[2]) {
        my $byte = ' ';
        
        $status = $self->scan(\$byte, $_[1]) ;
    }
    
    return $status ;
}

sub Compress::Raw::Zlib::deflateStream::deflateParams
{
    my $self = shift ;
    my ($got) = ParseParameters(0, {
                'Level'      => [1, 1, Parse_signed,   undef],
                'Strategy'   => [1, 1, Parse_unsigned, undef],
                'Bufsize'    => [1, 1, Parse_unsigned, undef],
                }, 
                @_) ;

    croak "Compress::Raw::Zlib::deflateParams needs Level and/or Strategy"
        unless $got->parsed('Level') + $got->parsed('Strategy') +
            $got->parsed('Bufsize');

    croak "Compress::Raw::Zlib::Inflate::deflateParams: Bufsize must be >= 1, you specified " . 
            $got->value('Bufsize')
        if $got->parsed('Bufsize') && $got->value('Bufsize') <= 1;

    my $flags = 0;
    $flags |= 1 if $got->parsed('Level') ;
    $flags |= 2 if $got->parsed('Strategy') ;
    $flags |= 4 if $got->parsed('Bufsize') ;

    $self->_deflateParams($flags, $got->value('Level'), 
                          $got->value('Strategy'), $got->value('Bufsize'));

}


# Autoload methods go after __END__, and are processed by the autosplit program.

1;
__END__


=head1 NAME

Compress::Raw::Zlib - Low-Level Interface to zlib compression library

=head1 SYNOPSIS

    use Compress::Raw::Zlib ;

    ($d, $status) = new Compress::Raw::Zlib::Deflate( [OPT] ) ;
    $status = $d->deflate($input, $output) ;
    $status = $d->flush($output [, $flush_type]) ;
    $d->deflateParams(OPTS) ;
    $d->deflateTune(OPTS) ;
    $d->dict_adler() ;
    $d->crc32() ;
    $d->adler32() ;
    $d->total_in() ;
    $d->total_out() ;
    $d->msg() ;
    $d->get_Strategy();
    $d->get_Level();
    $d->get_BufSize();

    ($i, $status) = new Compress::Raw::Zlib::Inflate( [OPT] ) ;
    $status = $i->inflate($input, $output [, $eof]) ;
    $status = $i->inflateSync($input) ;
    $i->dict_adler() ;
    $d->crc32() ;
    $d->adler32() ;
    $i->total_in() ;
    $i->total_out() ;
    $i->msg() ;
    $d->get_BufSize();

    $crc = adler32($buffer [,$crc]) ;
    $crc = crc32($buffer [,$crc]) ;

    $crc = adler32_combine($crc1, $crc2, $len2)l
    $crc = crc32_combine($adler1, $adler2, $len2)

    ZLIB_VERSION
    ZLIB_VERNUM

=head1 DESCRIPTION

The I<Compress::Raw::Zlib> module provides a Perl interface to the I<zlib>
compression library (see L</AUTHOR> for details about where to get
I<zlib>). 

=head1 Compress::Raw::Zlib::Deflate

This section defines an interface that allows in-memory compression using
the I<deflate> interface provided by zlib.

Here is a definition of the interface available:

=head2 B<($d, $status) = new Compress::Raw::Zlib::Deflate( [OPT] ) >

Initialises a deflation object. 

If you are familiar with the I<zlib> library, it combines the
features of the I<zlib> functions C<deflateInit>, C<deflateInit2>
and C<deflateSetDictionary>.

If successful, it will return the initialised deflation object, C<$d>
and a C<$status> of C<Z_OK> in a list context. In scalar context it
returns the deflation object, C<$d>, only.

If not successful, the returned deflation object, C<$d>, will be
I<undef> and C<$status> will hold the a I<zlib> error code.

The function optionally takes a number of named options specified as
C<< Name => value >> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.

For backward compatibility, it is also possible to pass the parameters
as a reference to a hash containing the name=>value pairs.

Below is a list of the valid options:

=over 5

=item B<-Level>

Defines the compression level. Valid values are 0 through 9,
C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>, C<Z_BEST_COMPRESSION>, and
C<Z_DEFAULT_COMPRESSION>.

The default is Z_DEFAULT_COMPRESSION.

=item B<-Method>

Defines the compression method. The only valid value at present (and
the default) is Z_DEFLATED.

=item B<-WindowBits>

For a definition of the meaning and valid values for C<WindowBits>
refer to the I<zlib> documentation for I<deflateInit2>.

Defaults to MAX_WBITS.

=item B<-MemLevel>

For a definition of the meaning and valid values for C<MemLevel>
refer to the I<zlib> documentation for I<deflateInit2>.

Defaults to MAX_MEM_LEVEL.

=item B<-Strategy>

Defines the strategy used to tune the compression. The valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED>, C<Z_RLE>, C<Z_FIXED> and
C<Z_HUFFMAN_ONLY>.

The default is Z_DEFAULT_STRATEGY.

=item B<-Dictionary>

When a dictionary is specified I<Compress::Raw::Zlib> will automatically
call C<deflateSetDictionary> directly after calling C<deflateInit>. The
Adler32 value for the dictionary can be obtained by calling the method 
C<$d-E<gt>dict_adler()>.

The default is no dictionary.

=item B<-Bufsize>

Sets the initial size for the output buffer used by the C<$d-E<gt>deflate>
and C<$d-E<gt>flush> methods. If the buffer has to be
reallocated to increase the size, it will grow in increments of
C<Bufsize>.

The default buffer size is 4096.

=item B<-AppendOutput>

This option controls how data is written to the output buffer by the
C<$d-E<gt>deflate> and C<$d-E<gt>flush> methods.

If the C<AppendOutput> option is set to false, the output buffers in the
C<$d-E<gt>deflate> and C<$d-E<gt>flush>  methods will be truncated before
uncompressed data is written to them.

If the option is set to true, uncompressed data will be appended to the
output buffer in the C<$d-E<gt>deflate> and C<$d-E<gt>flush> methods.

This option defaults to false.

=item B<-CRC32>

If set to true, a crc32 checksum of the uncompressed data will be
calculated. Use the C<$d-E<gt>crc32> method to retrieve this value.

This option defaults to false.

=item B<-ADLER32>

If set to true, an adler32 checksum of the uncompressed data will be
calculated. Use the C<$d-E<gt>adler32> method to retrieve this value.

This option defaults to false.

=back

Here is an example of using the C<Compress::Raw::Zlib::Deflate> optional
parameter list to override the default buffer size and compression
level. All other options will take their default values.

    my $d = new Compress::Raw::Zlib::Deflate ( -Bufsize => 300, 
                                               -Level   => Z_BEST_SPEED ) ;

=head2 B<$status = $d-E<gt>deflate($input, $output)>

Deflates the contents of C<$input> and writes the compressed data to
C<$output>.

The C<$input> and C<$output> parameters can be either scalars or scalar
references.

When finished, C<$input> will be completely processed (assuming there
were no errors). If the deflation was successful it writes the deflated
data to C<$output> and returns a status value of C<Z_OK>.

On error, it returns a I<zlib> error code.

If the C<AppendOutput> option is set to true in the constructor for
the C<$d> object, the compressed data will be appended to C<$output>. If
it is false, C<$output> will be truncated before any compressed data is
written to it.

B<Note>: This method will not necessarily write compressed data to
C<$output> every time it is called. So do not assume that there has been
an error if the contents of C<$output> is empty on returning from
this method. As long as the return code from the method is C<Z_OK>,
the deflate has succeeded.

=head2 B<$status = $d-E<gt>flush($output [, $flush_type]) >

Typically used to finish the deflation. Any pending output will be
written to C<$output>.

Returns C<Z_OK> if successful.

Note that flushing can seriously degrade the compression ratio, so it
should only be used to terminate a decompression (using C<Z_FINISH>) or
when you want to create a I<full flush point> (using C<Z_FULL_FLUSH>).

By default the C<flush_type> used is C<Z_FINISH>. Other valid values
for C<flush_type> are C<Z_NO_FLUSH>, C<Z_PARTIAL_FLUSH>, C<Z_SYNC_FLUSH>
and C<Z_FULL_FLUSH>. It is strongly recommended that you only set the
C<flush_type> parameter if you fully understand the implications of
what it does. See the C<zlib> documentation for details.

If the C<AppendOutput> option is set to true in the constructor for
the C<$d> object, the compressed data will be appended to C<$output>. If
it is false, C<$output> will be truncated before any compressed data is
written to it.

=head2 B<$status = $d-E<gt>deflateParams([OPT])>

Change settings for the deflate object C<$d>.

The list of the valid options is shown below. Options not specified
will remain unchanged.

=over 5

=item B<-Level>

Defines the compression level. Valid values are 0 through 9,
C<Z_NO_COMPRESSION>, C<Z_BEST_SPEED>, C<Z_BEST_COMPRESSION>, and
C<Z_DEFAULT_COMPRESSION>.

=item B<-Strategy>

Defines the strategy used to tune the compression. The valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED> and C<Z_HUFFMAN_ONLY>. 

=item B<-BufSize>

Sets the initial size for the output buffer used by the C<$d-E<gt>deflate>
and C<$d-E<gt>flush> methods. If the buffer has to be
reallocated to increase the size, it will grow in increments of
C<Bufsize>.

=back

=head2 B<$status = $d-E<gt>deflateTune($good_length, $max_lazy, $nice_length, $max_chain)>

Tune the internal settings for the deflate object C<$d>. This option is
only available if you are running zlib 1.2.2.3 or better.

Refer to the documentation in zlib.h for instructions on how to fly
C<deflateTune>.

=head2 B<$d-E<gt>dict_adler()>

Returns the adler32 value for the dictionary.

=head2 B<$d-E<gt>crc32()>

Returns the crc32 value for the uncompressed data to date. 

If the C<CRC32> option is not enabled in the constructor for this object,
this method will always return 0;

=head2 B<$d-E<gt>adler32()>

Returns the adler32 value for the uncompressed data to date. 

=head2 B<$d-E<gt>msg()>

Returns the last error message generated by zlib.

=head2 B<$d-E<gt>total_in()>

Returns the total number of bytes uncompressed bytes input to deflate.

=head2 B<$d-E<gt>total_out()>

Returns the total number of compressed bytes output from deflate.

=head2 B<$d-E<gt>get_Strategy()>

Returns the deflation strategy currently used. Valid values are
C<Z_DEFAULT_STRATEGY>, C<Z_FILTERED> and C<Z_HUFFMAN_ONLY>. 

=head2 B<$d-E<gt>get_Level()>

Returns the compression level being used. 

=head2 B<$d-E<gt>get_BufSize()>

Returns the buffer size used to carry out the compression.

=head2 Example

Here is a trivial example of using C<deflate>. It simply reads standard
input, deflates it and writes it to standard output.

    use strict ;
    use warnings ;

    use Compress::Raw::Zlib ;

    binmode STDIN;
    binmode STDOUT;
    my $x = new Compress::Raw::Zlib::Deflate
       or die "Cannot create a deflation stream\n" ;

    my ($output, $status) ;
    while (<>)
    {
        $status = $x->deflate($_, $output) ;
    
        $status == Z_OK
            or die "deflation failed\n" ;
    
        print $output ;
    }
    
    $status = $x->flush($output) ;
    
    $status == Z_OK
        or die "deflation failed\n" ;
    
    print $output ;

=head1 Compress::Raw::Zlib::Inflate

This section defines an interface that allows in-memory uncompression using
the I<inflate> interface provided by zlib.

Here is a definition of the interface:

=head2 B< ($i, $status) = new Compress::Raw::Zlib::Inflate( [OPT] ) >

Initialises an inflation object. 

In a list context it returns the inflation object, C<$i>, and the
I<zlib> status code (C<$status>). In a scalar context it returns the
inflation object only.

If successful, C<$i> will hold the inflation object and C<$status> will
be C<Z_OK>.

If not successful, C<$i> will be I<undef> and C<$status> will hold the
I<zlib> error code.

The function optionally takes a number of named options specified as
C<< -Name => value >> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.

For backward compatibility, it is also possible to pass the parameters
as a reference to a hash containing the C<< name=>value >> pairs.

Here is a list of the valid options:

=over 5

=item B<-WindowBits>

To uncompress an RFC 1950 data stream, set C<WindowBits> to a positive
number.

To uncompress an RFC 1951 data stream, set C<WindowBits> to C<-MAX_WBITS>.

For a full definition of the meaning and valid values for C<WindowBits>
refer to the I<zlib> documentation for I<inflateInit2>.

Defaults to MAX_WBITS.

=item B<-Bufsize>

Sets the initial size for the output buffer used by the C<$i-E<gt>inflate>
method. If the output buffer in this method has to be reallocated to
increase the size, it will grow in increments of C<Bufsize>.

Default is 4096.

=item B<-Dictionary>

The default is no dictionary.

=item B<-AppendOutput>

This option controls how data is written to the output buffer by the
C<$i-E<gt>inflate> method.

If the option is set to false, the output buffer in the C<$i-E<gt>inflate>
method will be truncated before uncompressed data is written to it.

If the option is set to true, uncompressed data will be appended to the
output buffer by the C<$i-E<gt>inflate> method.

This option defaults to false.

=item B<-CRC32>

If set to true, a crc32 checksum of the uncompressed data will be
calculated. Use the C<$i-E<gt>crc32> method to retrieve this value.

This option defaults to false.

=item B<-ADLER32>

If set to true, an adler32 checksum of the uncompressed data will be
calculated. Use the C<$i-E<gt>adler32> method to retrieve this value.

This option defaults to false.

=item B<-ConsumeInput>

If set to true, this option will remove compressed data from the input
buffer of the the C< $i-E<gt>inflate > method as the inflate progresses.

This option can be useful when you are processing compressed data that is
embedded in another file/buffer. In this case the data that immediately
follows the compressed stream will be left in the input buffer.

This option defaults to true.

=back

Here is an example of using an optional parameter to override the default
buffer size.

    my ($i, $status) = new Compress::Raw::Zlib::Inflate( -Bufsize => 300 ) ;

=head2 B< $status = $i-E<gt>inflate($input, $output [,$eof]) >

Inflates the complete contents of C<$input> and writes the uncompressed
data to C<$output>. The C<$input> and C<$output> parameters can either be
scalars or scalar references.

Returns C<Z_OK> if successful and C<Z_STREAM_END> if the end of the
compressed data has been successfully reached. 

If not successful C<$status> will hold the I<zlib> error code.

If the C<ConsumeInput> option has been set to true when the
C<Compress::Raw::Zlib::Inflate> object is created, the C<$input> parameter
is modified by C<inflate>. On completion it will contain what remains
of the input buffer after inflation. In practice, this means that when
the return status is C<Z_OK> the C<$input> parameter will contain an
empty string, and when the return status is C<Z_STREAM_END> the C<$input>
parameter will contains what (if anything) was stored in the input buffer
after the deflated data stream.

This feature is useful when processing a file format that encapsulates
a compressed data stream (e.g. gzip, zip) and there is useful data
immediately after the deflation stream.

If the C<AppendOutput> option is set to true in the constructor for
this object, the uncompressed data will be appended to C<$output>. If
it is false, C<$output> will be truncated before any uncompressed data
is written to it.

The C<$eof> parameter needs a bit of explanation. 

Prior to version 1.2.0, zlib assumed that there was at least one trailing
byte immediately after the compressed data stream when it was carrying out
decompression. This normally isn't a problem because the majority of zlib
applications guarantee that there will be data directly after the
compressed data stream.  For example, both gzip (RFC 1950) and zip both
define trailing data that follows the compressed data stream.

The C<$eof> parameter only needs to be used if B<all> of the following
conditions apply

=over 5

=item 1 

You are either using a copy of zlib that is older than version 1.2.0 or you
want your application code to be able to run with as many different
versions of zlib as possible.

=item 2

You have set the C<WindowBits> parameter to C<-MAX_WBITS> in the constructor
for this object, i.e. you are uncompressing a raw deflated data stream
(RFC 1951).

=item 3

There is no data immediately after the compressed data stream.

=back

If B<all> of these are the case, then you need to set the C<$eof> parameter
to true on the final call (and only the final call) to C<$i-E<gt>inflate>. 

If you have built this module with zlib >= 1.2.0, the C<$eof> parameter is
ignored. You can still set it if you want, but it won't be used behind the
scenes.

=head2 B<$status = $i-E<gt>inflateSync($input)>

This method can be used to attempt to recover good data from a compressed
data stream that is partially corrupt.
It scans C<$input> until it reaches either a I<full flush point> or the
end of the buffer.

If a I<full flush point> is found, C<Z_OK> is returned and C<$input>
will be have all data up to the flush point removed. This data can then be
passed to the C<$i-E<gt>inflate> method to be uncompressed.

Any other return code means that a flush point was not found. If more
data is available, C<inflateSync> can be called repeatedly with more
compressed data until the flush point is found.

Note I<full flush points> are not present by default in compressed
data streams. They must have been added explicitly when the data stream
was created by calling C<Compress::Deflate::flush>  with C<Z_FULL_FLUSH>.

=head2 B<$i-E<gt>dict_adler()>

Returns the adler32 value for the dictionary.

=head2 B<$i-E<gt>crc32()>

Returns the crc32 value for the uncompressed data to date.

If the C<CRC32> option is not enabled in the constructor for this object,
this method will always return 0;

=head2 B<$i-E<gt>adler32()>

Returns the adler32 value for the uncompressed data to date.

If the C<ADLER32> option is not enabled in the constructor for this object,
this method will always return 0;

=head2 B<$i-E<gt>msg()>

Returns the last error message generated by zlib.

=head2 B<$i-E<gt>total_in()>

Returns the total number of bytes compressed bytes input to inflate.

=head2 B<$i-E<gt>total_out()>

Returns the total number of uncompressed bytes output from inflate.

=head2 B<$d-E<gt>get_BufSize()>

Returns the buffer size used to carry out the decompression.

=head2 Example

Here is an example of using C<inflate>.

    use strict ;
    use warnings ;
    
    use Compress::Raw::Zlib;
    
    my $x = new Compress::Raw::Zlib::Inflate()
       or die "Cannot create a inflation stream\n" ;
    
    my $input = '' ;
    binmode STDIN;
    binmode STDOUT;
    
    my ($output, $status) ;
    while (read(STDIN, $input, 4096))
    {
        $status = $x->inflate(\$input, $output) ;
    
        print $output 
            if $status == Z_OK or $status == Z_STREAM_END ;
    
        last if $status != Z_OK ;
    }
    
    die "inflation failed\n"
        unless $status == Z_STREAM_END ;

=head1 CHECKSUM FUNCTIONS

Two functions are provided by I<zlib> to calculate checksums. For the
Perl interface, the order of the two parameters in both functions has
been reversed. This allows both running checksums and one off
calculations to be done.

    $crc = adler32($buffer [,$crc]) ;
    $crc = crc32($buffer [,$crc]) ;

The buffer parameters can either be a scalar or a scalar reference.

If the $crc parameters is C<undef>, the crc value will be reset.

If you have built this module with zlib 1.2.3 or better, two more
CRC-related functions are available.

    $crc = adler32_combine($crc1, $crc2, $len2)l
    $crc = crc32_combine($adler1, $adler2, $len2)

These functions allow checksums to be merged.

=head1 ACCESSING ZIP FILES

Although it is possible (with some effort on your part) to use this
module to access .zip files, there is a module on CPAN that will do all
the hard work for you. Check out the C<Archive::Zip> module on CPAN at

    http://www.cpan.org/modules/by-module/Archive/Archive-Zip-*.tar.gz    

=head1 CONSTANTS

All the I<zlib> constants are automatically imported when you make use
of I<Compress::Raw::Zlib>.

=head1 SEE ALSO

L<Compress::Zlib>, L<IO::Compress::Gzip>, L<IO::Uncompress::Gunzip>, L<IO::Compress::Deflate>, L<IO::Uncompress::Inflate>, L<IO::Compress::RawDeflate>, L<IO::Uncompress::RawInflate>, L<IO::Compress::Bzip2>, L<IO::Uncompress::Bunzip2>, L<IO::Compress::Lzop>, L<IO::Uncompress::UnLzop>, L<IO::Compress::Lzf>, L<IO::Uncompress::UnLzf>, L<IO::Uncompress::AnyInflate>, L<IO::Uncompress::AnyUncompress>

L<Compress::Zlib::FAQ|Compress::Zlib::FAQ>

L<File::GlobMapper|File::GlobMapper>, L<Archive::Zip|Archive::Zip>,
L<Archive::Tar|Archive::Tar>,
L<IO::Zlib|IO::Zlib>

For RFC 1950, 1951 and 1952 see 
F<http://www.faqs.org/rfcs/rfc1950.html>,
F<http://www.faqs.org/rfcs/rfc1951.html> and
F<http://www.faqs.org/rfcs/rfc1952.html>

The I<zlib> compression library was written by Jean-loup Gailly
F<gzip@prep.ai.mit.edu> and Mark Adler F<madler@alumni.caltech.edu>.

The primary site for the I<zlib> compression library is
F<http://www.zlib.org>.

The primary site for gzip is F<http://www.gzip.org>.

=head1 AUTHOR

This module was written by Paul Marquess, F<pmqs@cpan.org>. 

=head1 MODIFICATION HISTORY

See the Changes file.

=head1 COPYRIGHT AND LICENSE

Copyright (c) 2005-2008 Paul Marquess. All rights reserved.

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

