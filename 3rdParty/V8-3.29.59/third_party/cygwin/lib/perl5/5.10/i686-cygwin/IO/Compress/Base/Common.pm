package IO::Compress::Base::Common;

use strict ;
use warnings;
use bytes;

use Carp;
use Scalar::Util qw(blessed readonly);
use File::GlobMapper;

require Exporter;
our ($VERSION, @ISA, @EXPORT, %EXPORT_TAGS, $HAS_ENCODE);
@ISA = qw(Exporter);
$VERSION = '2.011';

@EXPORT = qw( isaFilehandle isaFilename whatIsInput whatIsOutput 
              isaFileGlobString cleanFileGlobString oneTarget
              setBinModeInput setBinModeOutput
              ckInOutParams 
              createSelfTiedObject
              getEncoding

              WANT_CODE
              WANT_EXT
              WANT_UNDEF
              WANT_HASH

              STATUS_OK
              STATUS_ENDSTREAM
              STATUS_EOF
              STATUS_ERROR
          );  

%EXPORT_TAGS = ( Status => [qw( STATUS_OK
                                 STATUS_ENDSTREAM
                                 STATUS_EOF
                                 STATUS_ERROR
                           )]);

                       
use constant STATUS_OK        => 0;
use constant STATUS_ENDSTREAM => 1;
use constant STATUS_EOF       => 2;
use constant STATUS_ERROR     => -1;
          
sub hasEncode()
{
    if (! defined $HAS_ENCODE) {
        eval
        {
            require Encode;
            Encode->import();
        };

        $HAS_ENCODE = $@ ? 0 : 1 ;
    }

    return $HAS_ENCODE;
}

sub getEncoding($$$)
{
    my $obj = shift;
    my $class = shift ;
    my $want_encoding = shift ;

    $obj->croakError("$class: Encode module needed to use -Encode")
        if ! hasEncode();

    my $encoding = Encode::find_encoding($want_encoding);

    $obj->croakError("$class: Encoding '$want_encoding' is not available")
       if ! $encoding;

    return $encoding;
}

our ($needBinmode);
$needBinmode = ($^O eq 'MSWin32' || 
                    ($] >= 5.006 && eval ' ${^UNICODE} || ${^UTF8LOCALE} '))
                    ? 1 : 1 ;

sub setBinModeInput($)
{
    my $handle = shift ;

    binmode $handle 
        if  $needBinmode;
}

sub setBinModeOutput($)
{
    my $handle = shift ;

    binmode $handle 
        if  $needBinmode;
}

sub isaFilehandle($)
{
    use utf8; # Pragma needed to keep Perl 5.6.0 happy
    return (defined $_[0] and 
             (UNIVERSAL::isa($_[0],'GLOB') or 
              UNIVERSAL::isa($_[0],'IO::Handle') or
              UNIVERSAL::isa(\$_[0],'GLOB')) 
          )
}

sub isaFilename($)
{
    return (defined $_[0] and 
           ! ref $_[0]    and 
           UNIVERSAL::isa(\$_[0], 'SCALAR'));
}

sub isaFileGlobString
{
    return defined $_[0] && $_[0] =~ /^<.*>$/;
}

sub cleanFileGlobString
{
    my $string = shift ;

    $string =~ s/^\s*<\s*(.*)\s*>\s*$/$1/;

    return $string;
}

use constant WANT_CODE  => 1 ;
use constant WANT_EXT   => 2 ;
use constant WANT_UNDEF => 4 ;
#use constant WANT_HASH  => 8 ;
use constant WANT_HASH  => 0 ;

sub whatIsInput($;$)
{
    my $got = whatIs(@_);
    
    if (defined $got && $got eq 'filename' && defined $_[0] && $_[0] eq '-')
    {
        #use IO::File;
        $got = 'handle';
        $_[0] = *STDIN;
        #$_[0] = new IO::File("<-");
    }

    return $got;
}

sub whatIsOutput($;$)
{
    my $got = whatIs(@_);
    
    if (defined $got && $got eq 'filename' && defined $_[0] && $_[0] eq '-')
    {
        $got = 'handle';
        $_[0] = *STDOUT;
        #$_[0] = new IO::File(">-");
    }
    
    return $got;
}

sub whatIs ($;$)
{
    return 'handle' if isaFilehandle($_[0]);

    my $wantCode = defined $_[1] && $_[1] & WANT_CODE ;
    my $extended = defined $_[1] && $_[1] & WANT_EXT ;
    my $undef    = defined $_[1] && $_[1] & WANT_UNDEF ;
    my $hash     = defined $_[1] && $_[1] & WANT_HASH ;

    return 'undef'  if ! defined $_[0] && $undef ;

    if (ref $_[0]) {
        return ''       if blessed($_[0]); # is an object
        #return ''       if UNIVERSAL::isa($_[0], 'UNIVERSAL'); # is an object
        return 'buffer' if UNIVERSAL::isa($_[0], 'SCALAR');
        return 'array'  if UNIVERSAL::isa($_[0], 'ARRAY')  && $extended ;
        return 'hash'   if UNIVERSAL::isa($_[0], 'HASH')   && $hash ;
        return 'code'   if UNIVERSAL::isa($_[0], 'CODE')   && $wantCode ;
        return '';
    }

    return 'fileglob' if $extended && isaFileGlobString($_[0]);
    return 'filename';
}

sub oneTarget
{
    return $_[0] =~ /^(code|handle|buffer|filename)$/;
}

sub IO::Compress::Base::Validator::new
{
    my $class = shift ;

    my $Class = shift ;
    my $error_ref = shift ;
    my $reportClass = shift ;

    my %data = (Class       => $Class, 
                Error       => $error_ref,
                reportClass => $reportClass, 
               ) ;

    my $obj = bless \%data, $class ;

    local $Carp::CarpLevel = 1;

    my $inType    = $data{inType}    = whatIsInput($_[0], WANT_EXT|WANT_HASH);
    my $outType   = $data{outType}   = whatIsOutput($_[1], WANT_EXT|WANT_HASH);

    my $oneInput  = $data{oneInput}  = oneTarget($inType);
    my $oneOutput = $data{oneOutput} = oneTarget($outType);

    if (! $inType)
    {
        $obj->croakError("$reportClass: illegal input parameter") ;
        #return undef ;
    }    

#    if ($inType eq 'hash')
#    {
#        $obj->{Hash} = 1 ;
#        $obj->{oneInput} = 1 ;
#        return $obj->validateHash($_[0]);
#    }

    if (! $outType)
    {
        $obj->croakError("$reportClass: illegal output parameter") ;
        #return undef ;
    }    


    if ($inType ne 'fileglob' && $outType eq 'fileglob')
    {
        $obj->croakError("Need input fileglob for outout fileglob");
    }    

#    if ($inType ne 'fileglob' && $outType eq 'hash' && $inType ne 'filename' )
#    {
#        $obj->croakError("input must ne filename or fileglob when output is a hash");
#    }    

    if ($inType eq 'fileglob' && $outType eq 'fileglob')
    {
        $data{GlobMap} = 1 ;
        $data{inType} = $data{outType} = 'filename';
        my $mapper = new File::GlobMapper($_[0], $_[1]);
        if ( ! $mapper )
        {
            return $obj->saveErrorString($File::GlobMapper::Error) ;
        }
        $data{Pairs} = $mapper->getFileMap();

        return $obj;
    }
    
    $obj->croakError("$reportClass: input and output $inType are identical")
        if $inType eq $outType && $_[0] eq $_[1] && $_[0] ne '-' ;

    if ($inType eq 'fileglob') # && $outType ne 'fileglob'
    {
        my $glob = cleanFileGlobString($_[0]);
        my @inputs = glob($glob);

        if (@inputs == 0)
        {
            # TODO -- legal or die?
            die "globmap matched zero file -- legal or die???" ;
        }
        elsif (@inputs == 1)
        {
            $obj->validateInputFilenames($inputs[0])
                or return undef;
            $_[0] = $inputs[0]  ;
            $data{inType} = 'filename' ;
            $data{oneInput} = 1;
        }
        else
        {
            $obj->validateInputFilenames(@inputs)
                or return undef;
            $_[0] = [ @inputs ] ;
            $data{inType} = 'filenames' ;
        }
    }
    elsif ($inType eq 'filename')
    {
        $obj->validateInputFilenames($_[0])
            or return undef;
    }
    elsif ($inType eq 'array')
    {
        $data{inType} = 'filenames' ;
        $obj->validateInputArray($_[0])
            or return undef ;
    }

    return $obj->saveErrorString("$reportClass: output buffer is read-only")
        if $outType eq 'buffer' && readonly(${ $_[1] });

    if ($outType eq 'filename' )
    {
        $obj->croakError("$reportClass: output filename is undef or null string")
            if ! defined $_[1] || $_[1] eq ''  ;

        if (-e $_[1])
        {
            if (-d _ )
            {
                return $obj->saveErrorString("output file '$_[1]' is a directory");
            }
        }
    }
    
    return $obj ;
}

sub IO::Compress::Base::Validator::saveErrorString
{
    my $self   = shift ;
    ${ $self->{Error} } = shift ;
    return undef;
    
}

sub IO::Compress::Base::Validator::croakError
{
    my $self   = shift ;
    $self->saveErrorString($_[0]);
    croak $_[0];
}



sub IO::Compress::Base::Validator::validateInputFilenames
{
    my $self = shift ;

    foreach my $filename (@_)
    {
        $self->croakError("$self->{reportClass}: input filename is undef or null string")
            if ! defined $filename || $filename eq ''  ;

        next if $filename eq '-';

        if (! -e $filename )
        {
            return $self->saveErrorString("input file '$filename' does not exist");
        }

        if (-d _ )
        {
            return $self->saveErrorString("input file '$filename' is a directory");
        }

        if (! -r _ )
        {
            return $self->saveErrorString("cannot open file '$filename': $!");
        }
    }

    return 1 ;
}

sub IO::Compress::Base::Validator::validateInputArray
{
    my $self = shift ;

    if ( @{ $_[0] } == 0 )
    {
        return $self->saveErrorString("empty array reference") ;
    }    

    foreach my $element ( @{ $_[0] } )
    {
        my $inType  = whatIsInput($element);
    
        if (! $inType)
        {
            $self->croakError("unknown input parameter") ;
        }    
        elsif($inType eq 'filename')
        {
            $self->validateInputFilenames($element)
                or return undef ;
        }
        else
        {
            $self->croakError("not a filename") ;
        }
    }

    return 1 ;
}

#sub IO::Compress::Base::Validator::validateHash
#{
#    my $self = shift ;
#    my $href = shift ;
#
#    while (my($k, $v) = each %$href)
#    {
#        my $ktype = whatIsInput($k);
#        my $vtype = whatIsOutput($v, WANT_EXT|WANT_UNDEF) ;
#
#        if ($ktype ne 'filename')
#        {
#            return $self->saveErrorString("hash key not filename") ;
#        }    
#
#        my %valid = map { $_ => 1 } qw(filename buffer array undef handle) ;
#        if (! $valid{$vtype})
#        {
#            return $self->saveErrorString("hash value not ok") ;
#        }    
#    }
#
#    return $self ;
#}

sub createSelfTiedObject
{
    my $class = shift || (caller)[0] ;
    my $error_ref = shift ;

    my $obj = bless Symbol::gensym(), ref($class) || $class;
    tie *$obj, $obj if $] >= 5.005;
    *$obj->{Closed} = 1 ;
    $$error_ref = '';
    *$obj->{Error} = $error_ref ;
    my $errno = 0 ;
    *$obj->{ErrorNo} = \$errno ;

    return $obj;
}



#package Parse::Parameters ;
#
#
#require Exporter;
#our ($VERSION, @ISA, @EXPORT);
#$VERSION = '2.000_08';
#@ISA = qw(Exporter);

$EXPORT_TAGS{Parse} = [qw( ParseParameters 
                           Parse_any Parse_unsigned Parse_signed 
                           Parse_boolean Parse_custom Parse_string
                           Parse_multiple Parse_writable_scalar
                         )
                      ];              

push @EXPORT, @{ $EXPORT_TAGS{Parse} } ;

use constant Parse_any      => 0x01;
use constant Parse_unsigned => 0x02;
use constant Parse_signed   => 0x04;
use constant Parse_boolean  => 0x08;
use constant Parse_string   => 0x10;
use constant Parse_custom   => 0x12;

#use constant Parse_store_ref        => 0x100 ;
use constant Parse_multiple         => 0x100 ;
use constant Parse_writable         => 0x200 ;
use constant Parse_writable_scalar  => 0x400 | Parse_writable ;

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
    local $Carp::CarpLevel = 1 ;
    my $p = new IO::Compress::Base::Parameters() ;
    $p->parse(@_)
        or croak "$sub: $p->{Error}" ;

    return $p;
}

#package IO::Compress::Base::Parameters;

use strict;
use warnings;
use Carp;

sub IO::Compress::Base::Parameters::new
{
    my $class = shift ;

    my $obj = { Error => '',
                Got   => {},
              } ;

    #return bless $obj, ref($class) || $class || __PACKAGE__ ;
    return bless $obj, 'IO::Compress::Base::Parameters' ;
}

sub IO::Compress::Base::Parameters::setError
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
          
sub IO::Compress::Base::Parameters::parse
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
        return $_[0] 
            if UNIVERSAL::isa($_[0], "IO::Compress::Base::Parameters");
    
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
            $x = [ $x ]
                if $type & Parse_multiple;

            $got->{$key} = [0, $type, $value, $x, $first_only, $sticky] ;
        }

        $got->{$key}[OFF_PARSED] = 0 ;
    }

    my %parsed = ();
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
            my $parsed = $parsed{$canonkey};
            ++ $parsed{$canonkey};

            return $self->setError("Muliple instances of '$key' found") 
                if $parsed && $type & Parse_multiple == 0 ;

            my $s ;
            $self->_checkType($key, $value, $type, 1, \$s)
                or return undef ;

            $value = $$value ;
            if ($type & Parse_multiple) {
                $got->{$canonkey}[OFF_PARSED] = 1;
                push @{ $got->{$canonkey}[OFF_FIXED] }, $s ;
            }
            else {
                $got->{$canonkey} = [1, $type, $value, $s] ;
            }
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

sub IO::Compress::Base::Parameters::_checkType
{
    my $self = shift ;

    my $key   = shift ;
    my $value = shift ;
    my $type  = shift ;
    my $validate  = shift ;
    my $output  = shift;

    #local $Carp::CarpLevel = $level ;
    #print "PARSE $type $key $value $validate $sub\n" ;

    if ($type & Parse_writable_scalar)
    {
        return $self->setError("Parameter '$key' not writable")
            if $validate &&  readonly $$value ;

        if (ref $$value) 
        {
            return $self->setError("Parameter '$key' not a scalar reference")
                if $validate &&  ref $$value ne 'SCALAR' ;

            $$output = $$value ;
        }
        else  
        {
            return $self->setError("Parameter '$key' not a scalar")
                if $validate &&  ref $value ne 'SCALAR' ;

            $$output = $value ;
        }

        return 1;
    }

#    if ($type & Parse_store_ref)
#    {
#        #$value = $$value
#        #    if ref ${ $value } ;
#
#        $$output = $value ;
#        return 1;
#    }

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



sub IO::Compress::Base::Parameters::parsed
{
    my $self = shift ;
    my $name = shift ;

    return $self->{Got}{lc $name}[OFF_PARSED] ;
}

sub IO::Compress::Base::Parameters::value
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

sub IO::Compress::Base::Parameters::valueOrDefault
{
    my $self = shift ;
    my $name = shift ;
    my $default = shift ;

    my $value = $self->{Got}{lc $name}[OFF_DEFAULT] ;

    return $value if defined $value ;
    return $default ;
}

sub IO::Compress::Base::Parameters::wantValue
{
    my $self = shift ;
    my $name = shift ;

    return defined $self->{Got}{lc $name}[OFF_DEFAULT] ;

}

sub IO::Compress::Base::Parameters::clone
{
    my $self = shift ;
    my $obj = { };
    my %got ;

    while (my ($k, $v) = each %{ $self->{Got} }) {
        $got{$k} = [ @$v ];
    }

    $obj->{Error} = $self->{Error};
    $obj->{Got} = \%got ;

    return bless $obj, 'IO::Compress::Base::Parameters' ;
}

package U64;

use constant MAX32 => 0xFFFFFFFF ;
use constant LOW   => 0 ;
use constant HIGH  => 1;

sub new
{
    my $class = shift ;

    my $high = 0 ;
    my $low  = 0 ;

    if (@_ == 2) {
        $high = shift ;
        $low  = shift ;
    }
    elsif (@_ == 1) {
        $low  = shift ;
    }

    bless [$low, $high], $class;
}

sub newUnpack_V64
{
    my $string = shift;

    my ($low, $hi) = unpack "V V", $string ;
    bless [ $low, $hi ], "U64";
}

sub newUnpack_V32
{
    my $string = shift;

    my $low = unpack "V", $string ;
    bless [ $low, 0 ], "U64";
}

sub reset
{
    my $self = shift;
    $self->[HIGH] = $self->[LOW] = 0;
}

sub clone
{
    my $self = shift;
    bless [ @$self ], ref $self ;
}

sub getHigh
{
    my $self = shift;
    return $self->[HIGH];
}

sub getLow
{
    my $self = shift;
    return $self->[LOW];
}

sub get32bit
{
    my $self = shift;
    return $self->[LOW];
}

sub add
{
    my $self = shift;
    my $value = shift;

    if (ref $value eq 'U64') {
        $self->[HIGH] += $value->[HIGH] ;
        $value = $value->[LOW];
    }
     
    my $available = MAX32 - $self->[LOW] ;

    if ($value > $available) {
       ++ $self->[HIGH] ;
       $self->[LOW] = $value - $available - 1;
    }
    else {
       $self->[LOW] += $value ;
    }
}

sub equal
{
    my $self = shift;
    my $other = shift;

    return $self->[LOW]  == $other->[LOW] &&
           $self->[HIGH] == $other->[HIGH] ;
}

sub getPacked_V64
{
    my $self = shift;

    return pack "V V", @$self ;
}

sub getPacked_V32
{
    my $self = shift;

    return pack "V", $self->[LOW] ;
}

sub pack_V64
{
    my $low  = shift;

    return pack "V V", $low, 0;
}


package IO::Compress::Base::Common;

1;
