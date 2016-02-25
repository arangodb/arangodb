# File	   : Bzip2.pm
# Author   : Rob Janes
# Created  : 14 April 2005
# Modified : 9 Aug 2005
# Version  : 2.09
#
#     Copyright (c) 2005 Rob Janes. All rights reserved.
#     This program is free software; you can redistribute it and/or
#     modify it under the same terms as Perl itself.
#

package Compress::Bzip2;

use 5.006;

use strict;
use warnings;

use Carp;
use Getopt::Std;
use Fcntl qw(:DEFAULT :mode);

require Exporter;
use AutoLoader;

our @ISA = qw(Exporter);

# Items to export into callers namespace by default. Note: do not export
# names by default without a very good reason. Use EXPORT_OK instead.
# Do not simply export all your public functions/methods/constants.

# This allows declaration	use Compress::Bzip2 ':all';
# If you do not need this, moving things directly into @EXPORT or @EXPORT_OK
# will save memory.
our %EXPORT_TAGS =
    ( 'constants' => [ qw(
			  BZ_CONFIG_ERROR
			  BZ_DATA_ERROR
			  BZ_DATA_ERROR_MAGIC
			  BZ_FINISH
			  BZ_FINISH_OK
			  BZ_FLUSH
			  BZ_FLUSH_OK
			  BZ_IO_ERROR
			  BZ_MAX_UNUSED
			  BZ_MEM_ERROR
			  BZ_OK
			  BZ_OUTBUFF_FULL
			  BZ_PARAM_ERROR
			  BZ_RUN
			  BZ_RUN_OK
			  BZ_SEQUENCE_ERROR
			  BZ_STREAM_END
			  BZ_UNEXPECTED_EOF
			 ) ],

      'utilities' => [ qw(
			  &bzopen
			  &bzinflateInit
			  &bzdeflateInit
			  &memBzip &memBunzip
			  &compress &decompress
			  &bzip2 &bunzip2
			  &bzlibversion
			  $bzerrno
			  ) ],

      'bzip1' => [ qw(
		      &compress
		      &decompress
		      &compress_init
		      &decompress_init
		      &version
		      ) ],

      'gzip' => [ qw(
		     &gzopen
		     &inflateInit
		     &deflateInit
		     &compress &uncompress
		     &adler32 &crc32

		     ZLIB_VERSION

		     $gzerrno

		     Z_OK
		     Z_STREAM_END
		     Z_NEED_DICT
		     Z_ERRNO
		     Z_STREAM_ERROR
		     Z_DATA_ERROR
		     Z_MEM_ERROR
		     Z_BUF_ERROR
		     Z_VERSION_ERROR

		     Z_NO_FLUSH
		     Z_PARTIAL_FLUSH
		     Z_SYNC_FLUSH
		     Z_FULL_FLUSH
		     Z_FINISH
		     Z_BLOCK

		     Z_NO_COMPRESSION
		     Z_BEST_SPEED
		     Z_BEST_COMPRESSION
		     Z_DEFAULT_COMPRESSION

		     Z_FILTERED
		     Z_HUFFMAN_ONLY
		     Z_RLE
		     Z_DEFAULT_STRATEGY

		     Z_BINARY
		     Z_ASCII
		     Z_UNKNOWN

		     Z_DEFLATED
		     Z_NULL
		     ) ],
      );

our @EXPORT_OK = ( @{ $EXPORT_TAGS{'utilities'} },
		   @{ $EXPORT_TAGS{'constants'} },
		   @{ $EXPORT_TAGS{'bzip1'} },
		   @{ $EXPORT_TAGS{'gzip'} },
		   );

$EXPORT_TAGS{'all'} = [ @EXPORT_OK ];

our @EXPORT = ( @{ $EXPORT_TAGS{'utilities'} }, @{ $EXPORT_TAGS{'constants'} } );

our $VERSION = "2.09";

our $bzerrno = "";
our $gzerrno;
*gzerrno = \$bzerrno;

# Zlib compatibility
##
use constant ZLIB_VERSION => '1.x';
# allowed flush values
use constant { Z_NO_FLUSH => 0, Z_PARTIAL_FLUSH => 1, Z_SYNC_FLUSH => 2,
	       Z_FULL_FLUSH => 3, Z_FINISH => 4, Z_BLOCK => 5 };
# return codes for functions, positive normal, negative error
use constant { Z_OK => 0, Z_STREAM_END => 1, Z_NEED_DICT => 2, Z_ERRNO => -1,
	       Z_STREAM_ERROR => -2, Z_DATA_ERROR => -3, Z_MEM_ERROR => -4,
	       Z_BUF_ERROR => -5, Z_VERSION_ERROR => -6 };
# compression levels
use constant { Z_NO_COMPRESSION => 0, Z_BEST_SPEED => 1,
	       Z_BEST_COMPRESSION => 9, Z_DEFAULT_COMPRESSION => -1 };
# compression strategy, for deflateInit
use constant { Z_FILTERED => 1, Z_HUFFMAN_ONLY => 2, Z_RLE => 3,
	       Z_DEFAULT_STRATEGY => 0 };
# possible values of data_type (inflate)
use constant { Z_BINARY => 0, Z_ASCII => 1, Z_UNKNOWN => 2 };
# the deflate compression method
use constant Z_DEFLATED => 8;
# for initialization
use constant Z_NULL => 0;

## gzopen, $gzerror, gzerror, gzclose, gzreadline, gzwrite

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.

    my $constname;
    our $AUTOLOAD;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    croak "&Compress::Bzip2::constant not defined" if $constname eq 'constant';
    my ($error, $val) = constant($constname);
    if ($error) { croak $error; }
    {
	no strict 'refs';
	# Fixed between 5.005_53 and 5.005_61
#XXX	if ($] >= 5.00561) {
#XXX	    *$AUTOLOAD = sub () { $val };
#XXX	}
#XXX	else {
	    *$AUTOLOAD = sub { $val };
#XXX	}
    }
    goto &$AUTOLOAD;
}

require XSLoader;
XSLoader::load('Compress::Bzip2', $VERSION);

#bootstrap Compress::Bzip2 $VERSION;

##############################################################################
## file compress uncompress commands

sub _writefileopen ( $$;$ ) {
  ## open a protected file for write
  my ( $handle, $filename, $force ) = @_;

  if ( sysopen($handle, $filename, $force ? O_WRONLY|O_CREAT|O_TRUNC : O_WRONLY|O_CREAT|O_EXCL, S_IWUSR|S_IRUSR) ) {
    $_[0] = $handle if !defined($_[0]);
    return $handle;
  }

  return undef;
}

sub _stat_snapshot ( $ ) {
  my ( $filename ) = @_;
  return undef if !defined($filename);

  my @stats = stat $filename;
  if (!@stats) {
    warn "stat of $filename failed: $!\n" if !@stats;
    return undef;
  }

  return \@stats;
}

sub _check_stat ( $$;$ ) {
  my ( $filename, $statsnap, $force ) = @_;

  if ( !defined($statsnap) || (ref($statsnap) eq 'ARRAY' && @$statsnap == 0) ) {
    $statsnap = _stat_snapshot( $filename );
    if ( $statsnap ) {
      if ( @_>1 ) {
	if ( !defined($_[1]) ) {
	  $_[1] = $statsnap;
	}
	elsif ( ref($_[1]) eq 'ARRAY' && @{ $_[1] } == 0 ) {
	  @{ $_[1] } = @$statsnap;
	}
      }
    }
    else {
      return undef;
    }
  }

  if ( S_ISDIR( $statsnap->[2] ) ) {
    bz_seterror( &BZ_IO_ERROR, "file $filename is a directory" );
    return 0;
  }

  if ( !S_ISREG( $statsnap->[2] ) ) {
    bz_seterror( &BZ_IO_ERROR, "file $filename is not a normal file" );
    return 0;
  }

  if ( !$force && S_ISLNK( $statsnap->[2] ) ) {
    bz_seterror( &BZ_IO_ERROR, "file $filename is a symlink" );
    return 0;
  }

  if ( !$force && $statsnap->[3] > 1 ) {
    bz_seterror( &BZ_IO_ERROR, "file $filename has too many hard links" );
    return 0;
  }

  return 1;
}

sub _set_stat_from_snapshot ( $$ ) {
  my ( $filename, $statsnap ) = @_;

  if ( !chmod( S_IMODE( $statsnap->[2] ), $filename ) ) {
    bz_seterror( &BZ_IO_ERROR, "chmod ".sprintf('%03o', S_IMODE( $statsnap->[2] ))." $filename failed: $!" );
    return undef;
  }

  if ( !utime @$statsnap[8,9], $filename ) {
    bz_seterror( &BZ_IO_ERROR,
		 "utime " . join(' ',map { strftime('%Y-%m-%d %H:%M:%S', localtime $_) } @$statsnap[8,9] ) .
		 " $filename failed: $!" );
    return undef;
  }

  if ( !chown @$statsnap[4,5], $filename ) {
    bz_seterror( &BZ_IO_ERROR,
		 "chown " . join(':', ( getpwuid($statsnap->[4]) )[0], ( getgrgid($statsnap->[5]) )[0]) .
		 " $filename failed: $!" );
    return 0;
  }

  return 1;
}

sub bzip2 ( @ ) {
  return _process_files( 'bzip2', 'cfvks123456789', @_ );
}

sub bunzip2 ( @ ) {
  return _process_files( 'bunzip2', 'cdzfks123456789', @_ );
}

sub bzcat ( @ ) {
  return _process_files( 'bzcat', 'cdzfks123456789', @_ );
}

sub _process_files ( @ ) {
  my $command = shift;
  my $opts = shift;

  local @ARGV = @_;

  my %opts;
  return undef if !getopt( $opts, \%opts );
  # c compress or decompress to stdout
  # d decompress
  # z compress
  # f force
  # v verbose
  # k keep
  # s small
  # 123456789

  $opts{c} = 1 if $command eq 'bzcat';
  $opts{d} = 1 if $command eq 'bunzip2' || $command eq 'bzcat';
  $opts{z} = 1 if $command eq 'bzip2';

  my $read_from_stdin;
  my ( $in, $bzin );
  my ( $out, $bzout );

  if ( !@ARGV ) {
    $read_from_stdin = 1;
    $opts{c} = 1;
    if ( !open( $in, "<&STDIN" ) ) {
      die "Error: failed to input from STDIN: '$!'\n";
    }

    $bzin = bzopen( $in, "r" );
  }

  if ( $opts{c} ) {
    if ( !open( $out, ">&STDOUT" ) ) {
      die "Error: failed to output to STDOUT: '$!'\n";
    }

    $bzout = bzopen( $out, "w" );
  }

  if ( !$opts{d} && !$opts{z} ) {
    die "Error: neither compress nor decompress was indicated.\n";
  }

  my $doneflag = 0;
  while ( !$doneflag ) {
    my $infile;
    my $outfile;
    my @statbuf;

    if ( !$read_from_stdin ) {
      $infile = shift @ARGV;
      if ( ! -r $infile ) {
	print STDERR "Error: file $infile is not readable\n";
	next;
      }

      @statbuf = stat _;
      if ( !@statbuf ) {
	print STDERR "Error: failed to stat $infile: '$!'\n";
	next;
      }
      
      if ( !_check_stat( $infile, \@statbuf, $opts{f} ) ) {
	print STDERR "Error: file $infile stat check fails: $bzerrno\n";
	next;
      }
    }

    my $outfile_exists;
    if ( !$opts{c} ) {
      undef $out;
      if ( $opts{d} ) {
	$outfile = $infile . '.bz2';
      }
      elsif ( $opts{z} ) {
	$outfile = $infile =~ /\.bz2$/ ? substr($infile,0,-4) : $infile.'.out';
      }

      $outfile_exists = -e $outfile;
      if ( !_writefileopen( $out, $outfile, $opts{f} ) ) {
	print STDERR "Error: failed to open $outfile for write: '$!'\n";
	next;
      }
    }
    
    if ( !$read_from_stdin ) {
      undef $in;
      if ( !open( $in, $infile ) ) {
	print STDERR "Error: unable to open $infile: '$!'\n";
	unlink( $outfile ) if !$outfile_exists;
	next;
      }
    }

    if ( $opts{d} ) {
      $bzin = bzopen( $in, "r" ) if !$read_from_stdin;

      my $buf;
      my $notdone = 1;
      while ( $notdone ) {
	my $ln = bzread( $in, $buf, 1024 );
	if ( $ln > 0 ) {
	  syswrite( $out, $buf, $ln );
	}
	elsif ( $ln == 0 ) {
	  undef $notdone;
	}
	else {
	}
      }

      close($out);

      if ( !$read_from_stdin ) {
	bzclose($in);
	unlink( $infile ) if !$opts{k};
	_set_stat_from_snapshot( $outfile, \@statbuf );
      }
    }
    elsif ( $opts{z} ) {
      $bzout = bzopen( $out, "w" ) if !$opts{c};

      my $buf;
      my $notdone = 1;
      while ( $notdone ) {
	my $ln = sysread( $in, $buf, 1024 );
	if ( $ln > 0 ) {
	  bzwrite( $bzout, $buf, $ln );
	}
	elsif ( $ln == 0 ) {
	  undef $notdone;
	}
	else {
	}
      }

      close($in);

      if ( !$opts{c} ) {
	bzclose($bzout);
	unlink( $infile ) if !$opts{k};
	_set_stat_from_snapshot( $outfile, \@statbuf );
      }
    }
  }
}
  
##############################################################################
##############################################################################
## compatibility with Compress::Bzip2 1.03

sub add ( $$ ) {
  my ( $obj, $buffer ) = @_;

  my @res = $obj->is_write ? $obj->bzdeflate( $buffer ) : $obj->bzinflate( $buffer );

  return $res[0];
}

sub finish ( $;$ ) {
  my ( $obj, $buffer ) = @_;
  my ( @res, $out );

  if ( defined($buffer) ) {
    @res = $obj->is_write ? $obj->bzdeflate( $buffer ) : $obj->bzinflate( $buffer );
    return undef if $res[1] != &BZ_OK;

    $out = $res[0];
  }
  $out = '' if !defined($out);

  @res = $obj->bzclose;
  return undef if $res[1] != &BZ_OK;

  return $out.$res[0];
}

sub input_size ( $ ) {
  my ( $obj ) = @_;
  return $obj->total_in;
}

sub output_size ( $ ) {
  my ( $obj ) = @_;
  return $obj->total_out;
}

sub version ( ) {
  return bzlibversion();
}

sub error ( $ ) {
  return $_[0]->bzerror;
}

##############################################################################
##############################################################################
## THE Compress::Zlib compatibility section

sub _bzerror2gzerror {
  my ( $bz_error_num ) = @_;
  my $gz_error_num =
      $bz_error_num == &BZ_OK ? Z_OK :
      $bz_error_num == &BZ_RUN_OK ? Z_OK :
      $bz_error_num == &BZ_FLUSH_OK ? Z_STREAM_END :
      $bz_error_num == &BZ_FINISH_OK ? Z_STREAM_END :
      $bz_error_num == &BZ_STREAM_END ? Z_STREAM_END :

      $bz_error_num == &BZ_SEQUENCE_ERROR ? Z_VERSION_ERROR :
      $bz_error_num == &BZ_PARAM_ERROR ? Z_ERRNO :
      $bz_error_num == &BZ_MEM_ERROR ? Z_MEM_ERROR :
      $bz_error_num == &BZ_DATA_ERROR ? Z_DATA_ERROR :
      $bz_error_num == &BZ_DATA_ERROR_MAGIC ? Z_DATA_ERROR :
      $bz_error_num == &BZ_IO_ERROR ? Z_ERRNO :
      $bz_error_num == &BZ_UNEXPECTED_EOF ? Z_STREAM_ERROR :
      $bz_error_num == &BZ_OUTBUFF_FULL ? Z_BUF_ERROR :
      $bz_error_num == &BZ_CONFIG_ERROR ? Z_VERSION_ERROR :
      Z_VERSION_ERROR
      ;

  return $gz_error_num;
}

sub gzopen ( $$ ) {
  goto &bzopen;
}

sub gzread ( $$;$ ) {
  goto &bzread;
}

sub gzreadline ( $$ ) {
  goto &bzreadline;
}

sub gzwrite ( $$ ) {
  goto &bzwrite;
}

sub gzflush ( $;$ ) {
  my ( $obj, $flush ) = @_;
  return Z_OK if $flush == Z_NO_FLUSH;
  goto &bzflush;
}

sub gzclose ( $ ) {
  goto &bzclose;
}

sub gzeof ( $ ) {
  goto &bzeof;
}

sub gzsetparams ( $$$ ) {
  ## ignore params
  my ( $obj, $level, $strategy ) = @_;
  return Z_OK;
}

sub gzerror ( $ ) {
  goto &bzerror;
}

sub deflateInit ( @ ) {
  ## ignore all options:
  ## -Level, -Method, -WindowBits, -MemLevel, -Strategy, -Dictionary, -Bufsize

  my @res = bzdeflateInit();
  return $res[0] if !wantarray;

  return ( $res[0], _bzerror2gzerror( $res[1] ) );
}

sub deflate ( $$ ) {
  my ( $obj, $buffer ) = @_;

  my @res = $obj->bzdeflate( $buffer );

  return $res[0] if !wantarray;
  return ( $res[0], _bzerror2gzerror( $res[1] ) );
}

sub deflateParams ( $;@ ) {
  ## ignore all options
  return Z_OK;
}

sub flush ( $;$ ) {
  my ( $obj, $flush_type ) = @_;

  $flush_type = Z_FINISH if !defined($flush_type);
  return Z_OK if $flush_type == Z_NO_FLUSH;

  my $bz_flush_type;
  my @res;

  $bz_flush_type =
      $flush_type == Z_PARTIAL_FLUSH || $flush_type == Z_SYNC_FLUSH ? &BZ_FLUSH :
      $flush_type == Z_FULL_FLUSH ? &BZ_FINISH :
      &BZ_FINISH;

  @res = $obj->bzflush( $bz_flush_type );

  return $res[0] if !wantarray;
  return ( $res[0], _bzerror2gzerror( $res[1] ) );
}

sub dict_adler ( $ ) {
  return 1;			# ???
}

sub msg ( $ ) {
  my ( $obj ) = @_;

  return ''.($obj->bzerror).'';	# stringify
}

sub inflateInit ( @ ) {
  ## ignore all options:
  ## -WindowBits, -Dictionary, -Bufsize

  my @res = bzinflateInit();
  return $res[0] if !wantarray;

  return ( $res[0], _bzerror2gzerror( $res[1] ) );
}

sub inflate ( $$ ) {
  my ( $obj, $buffer ) = @_;

  my @res = $obj->bzinflate( $buffer );

  return $res[0] if !wantarray;
  return ( $res[0], _bzerror2gzerror( $res[1] ) );
}

sub inflateSync ( $ ) {
  return Z_VERSION_ERROR;	# ?? what
}

sub memGzip ( $ ) {
  goto &memBzip;
}

sub memGunzip ( $ ) {
  goto &memBunzip;
}

sub adler32 ( $;$ ) {
  return 0;
}

sub crc32 ( $;$ ) {
  return 0;
}

# sub compress ( $;$ ) {
#   ## ignore $level
#   my ( $source, $level ) = @_;
#   return memBzip( $source );
# }

sub uncompress ( $ ) {
  my ( $source, $level ) = @_;
  return memBunzip( $source );
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;

__END__

=pod

=head1 NAME

Compress::Bzip2 - Interface to Bzip2 compression library

=head1 SYNOPSIS

    use Compress::Bzip2 qw(:all :constant :utilities :gzip);

    ($bz, $status) = bzdeflateInit( [PARAMS] ) ;
    ($out, $status) = $bz->bzdeflate($buffer) ;

    ($bz, $status) = bzinflateInit( [PARAMS] ) ;
    ($out, $status) = $bz->bzinflate($buffer) ;

    ($out, $status) = $bz->bzflush() ;
    ($out, $status) = $bz->bzclose() ;

    $dest = memBzip($source);
        alias compress
    $dest = memBunzip($source);
        alias decompress

    $bz = Compress::Bzip2->new( [PARAMS] );

    $bz = bzopen($filename or filehandle, $mode);
        alternate, with $bz created by new():
    $bz->bzopen($filename or filehandle, $mode);

    $bytesread = $bz->bzread($buffer [,$size]) ;
    $bytesread = $bz->bzreadline($line);
    $byteswritten = $bz->bzwrite($buffer [,$limit]);
    $errstring = $bz->bzerror(); 
    $status = $bz->bzeof();
    $status = $bz->bzflush();
    $status = $bz->bzclose() ;

    $status = $bz->bzsetparams( $param => $setting );

    $bz->total_in() ;
    $bz->total_out() ;

    $verstring = $bz->bzversion();

    $Compress::Bzip2::bzerrno

=head1 DESCRIPTION

The I<Compress::Bzip2> module provides a Perl interface to the I<Bzip2>
compression library (see L</AUTHOR> for details about where to get
I<Bzip2>). A relevant subset of the functionality provided by I<Bzip2>
is available in I<Compress::Bzip2>.

All string parameters can either be a scalar or a scalar reference.

The module can be split into two general areas of functionality, namely
in-memory compression/decompression and read/write access to I<bzip2>
files. Each of these areas will be discussed separately below.

=head1 FILE READ/WRITE INTERFACE

A number of functions are supplied in I<bzlib> for reading and writing
I<bzip2> files. Unfortunately, most of them are not suitable.  So, this
module provides another interface, built over top of the low level bzlib
methods.

=over 5

=head2 B<$bz = bzopen(filename or filehandle, mode)>

This function returns an object which is used to access the other
I<bzip2> methods.

The B<mode> parameter is used to specify both whether the file is
opened for reading or writing, with "r" or "w" respectively.

If a reference to an open filehandle is passed in place of the
filename, it better be positioned to the start of a
compression/decompression sequence.

=head2 B<$bz = Compress::Bzip2-E<gt>new( [PARAMS] )>

Create a Compress::Bzip2 object.  Optionally, provide
compression/decompression parameters as a keyword => setting list.
See I<bzsetparams()> for a description of the parameters.

=head2 B<$bz-E<gt>bzopen(filename or filehandle, mode)>

This is bzopen, but it uses an object previously created by the new
method.  Other than that, it is identical to the above bzopen.

=head2 B<$bytesread = $bz-E<gt>bzread($buffer [, $size]) ;>

Reads B<$size> bytes from the compressed file into B<$buffer>. If
B<$size> is not specified, it will default to 4096. If the scalar
B<$buffer> is not large enough, it will be extended automatically.

Returns the number of bytes actually read. On EOF it returns 0 and in
the case of an error, -1.

=head2 B<$bytesread = $bz-E<gt>bzreadline($line) ;>

Reads the next line from the compressed file into B<$line>. 

Returns the number of bytes actually read. On EOF it returns 0 and in
the case of an error, -1.

It IS legal to intermix calls to B<bzread> and B<bzreadline>.

At this time B<bzreadline> ignores the variable C<$/>
(C<$INPUT_RECORD_SEPARATOR> or C<$RS> when C<English> is in use). The
end of a line is denoted by the C character C<'\n'>.

=head2 B<$byteswritten = $bz-E<gt>bzwrite($buffer [, $limit]) ;>

Writes the contents of B<$buffer> to the compressed file. Returns the
number of bytes actually written, or 0 on error.

If $limit is given and non-zero, then only that many bytes from
$buffer will be written.

=head2 B<$status = $bz-E<gt>bzflush($flush) ;>

Flushes all pending output to the compressed file.
Works identically to the I<zlib> function it interfaces to. Note that
the use of B<bzflush> can degrade compression.

Returns C<BZ_OK> if B<$flush> is C<BZ_FINISH> and all output could be
flushed. Otherwise the bzlib error code is returned.

Refer to the I<bzlib> documentation for the valid values of B<$flush>.

=head2 B<$status = $bz-E<gt>bzeof() ;>

Returns 1 if the end of file has been detected while reading the input
file, otherwise returns 0.

=head2 B<$bz-E<gt>bzclose>

Closes the compressed file. Any pending data is flushed to the file
before it is closed.

=head2 B<$bz-E<gt>bzsetparams( [PARAMS] );>

Change settings for the deflate stream C<$bz>.

The list of the valid options is shown below. Options not specified
will remain unchanged.

=over 5

=item B<-verbosity>

Defines the verbosity level. Valid values are 0 through 4,

The default is C<-verbosity =E<gt> 0>.

=item B<-blockSize100k>

For bzip object opened for stream deflation or write.

Defines the buffering factor of compression method.  The algorithm
buffers all data until the buffer is full, then it flushes all the
data out.  Use -blockSize100k to specify the size of the buffer.

Valid settings are 1 through 9, representing a blocking in multiples
of 100k.

Note that each such block has an overhead of leading and trailing
synchronization bytes.  bzip2 recovery uses this information to
pull useable data out of a corrupted file.

A streaming application would probably want to set the blocking low.

=item B<-workFactor>

For bzip object opened for stream deflation or write.

The workFactor setting tells the deflation algorithm how much work
to invest to compensate for repetitive data.

workFactor may be a number from 0 to 250 inclusive.  The default setting
is 30.

See the bzip documentation for more information.

=item B<-small>

For bzip object opened for stream inflation or read.

B<small> may be 0 or 1.  Set C<small> to one to use a slower, less
memory intensive algorithm.

=back

=head2 B<$bz-E<gt>bzerror>

Returns the I<bzlib> error message or number for the last operation
associated with B<$bz>. The return value will be the I<bzlib> error
number when used in a numeric context and the I<bzlib> error message
when used in a string context. The I<bzlib> error number constants,
shown below, are available for use.

  BZ_CONFIG_ERROR
  BZ_DATA_ERROR
  BZ_DATA_ERROR_MAGIC
  BZ_FINISH
  BZ_FINISH_OK
  BZ_FLUSH
  BZ_FLUSH_OK
  BZ_IO_ERROR
  BZ_MAX_UNUSED
  BZ_MEM_ERROR
  BZ_OK
  BZ_OUTBUFF_FULL
  BZ_PARAM_ERROR
  BZ_RUN
  BZ_RUN_OK
  BZ_SEQUENCE_ERROR
  BZ_STREAM_END
  BZ_UNEXPECTED_EOF

=head2 B<$bzerrno>

The B<$bzerrno> scalar holds the error code associated with the most
recent I<bzip2> routine. Note that unlike B<bzerror()>, the error is
I<not> associated with a particular file.

As with B<bzerror()> it returns an error number in numeric context and
an error message in string context. Unlike B<bzerror()> though, the
error message will correspond to the I<bzlib> message when the error is
associated with I<bzlib> itself, or the UNIX error message when it is
not (i.e. I<bzlib> returned C<Z_ERRORNO>).

As there is an overlap between the error numbers used by I<bzlib> and
UNIX, B<$bzerrno> should only be used to check for the presence of
I<an> error in numeric context. Use B<bzerror()> to check for specific
I<bzlib> errors. The I<bzcat> example below shows how the variable can
be used safely.

=back

=head1 Compress::Bzip2 1.03 COMPATIBILITY

While the 2.x thread forked off of 1.00, another line of development
came to a head at 1.03.  The 1.03 version worked with bzlib 1.0.2, had
improvements to the error handling, single buffer inflate/deflate, a
streaming interface to inflate/deflate, and a cpan style test suite.

=over 5

=head2 B<$dest = compress( $string, [$level] )>

Alias to memBzip, this compresses string, using the optional
compression level, 1 through 9, the default being 1.  Returns a string
containing the compressed data.

On error I<undef> is returned.

=head2 B<$dest = decompress($string)>

Alias to memBunzip, this decompresses the data in string, returning a
string containing the decompressed data.

On error I<undef> is returned.

=head2 B<$stream = compress_init( [PARAMS] )>

Alias to bzdeflateInit.  In addition to the named parameters
documented for bzdeflateInit, the following are accepted:

   -level, alias to -blockSize100k
   -buffer, to set the buffer size.

The -buffer option is ignored.  The intermediate buffer size is not
changeable.

=head2 B<$stream = decompress_init( [PARAMS] )>

Alias to bzinflateInit.  See bzinflateInit for a description of the parameters.
The option "-buffer" is accepted, but ignored.

=head2 B<$output = $stream-E<gt>add( $string )>

Add data to be compressed/decompressed.  Returns whatever output is available
(possibly none, if it's still buffering it), or undef on error.

=head2 B<$output = $stream-E<gt>finish( [$string] )>

Finish the operation; takes an optional final data string.  Whatever is
returned completes the output; returns undef on error.

=head2 B<$stream-E<gt>error>

Like the function, but applies to the current object only.  Note that errors
in a stream object are also returned by the function.

=head2 B<$stream-E<gt>input_size>

Alias to total_in.  Total bytes passed to the stream.

=head2 B<$stream-E<gt>output_size>

Alias to total_out.  Total bytes received from the stream.

=back

=head1 GZIP COMPATIBILITY INTERFACE

Except for the exact state and error numbers, this package presents an
interface very much like that given by the Compress::Zlib package.
Mostly, if you take the method name, state or error number from
Compress::Zlib and replace the "g" with a "b", your code should work.

To make the interoperability even easier, all the Compress::Zlib method
names have been used as aliases or cover functions for the bzip2 methods.

Therefore, most code that uses Compress::Zlib should be able to use
this package, with a one line change.

Simply change

   $gz = Compress::Zlib::gzopen( "filename", "w" );

to

   $gz = Compress::Bzip2::gzopen( "filename", "w" );

Some of the Compress::Zlib aliases don't return anything useful, like
crc32 or adler32, cause bzip2 doesn't do that sort of thing.

=over 5

=head2 B< $gz = gzopen( $filename, $mode ) >

Alias for bzopen.

=head2 B< $gz-E<gt>gzread( $buffer, [ $length ] ) >

Alias for bzread.

=head2 B< $gz-E<gt>gzreadline( $buffer ) >

Alias for bzreadline.

=head2 B< $gz-E<gt>gzwrite( $buffer ) >

Alias for bzwrite.

=head2 B< $gz-E<gt>gzflush( [$flushtype] ) >

Alias for bzflush, with return code translation.

=head2 B< $gz-E<gt>gzclose( ) >

Alias for bzclose.

=head2 B< $gz-E<gt>gzeof( ) >

Alias for bzeof.

=head2 B< $gz-E<gt>gzerror( ) >

Alias for bzerror.

=head2 B< $gz-E<gt>gzsetparams( $level, $strategy ) >

This is a no-op.

=head2 B< $d = deflateInit( [OPTS] ) >

Alias for bzdeflateInit, with return code translation.

All OPTS are ignored.

=head2 B< $d-E<gt>deflate( $buffer ) >

Alias for bzdeflate, with return code translation.

=head2 B< $d-E<gt>deflateParams( [OPTS] ) >

This is a no-op.

=head2 B< $d-E<gt>flush( [$flushtype] ) >

Cover function for bzflush or bzclose, depending on $flushtype.

See the Compress::Zlib documentation for more information.

=head2 B< $d-E<gt>dict_adler( ) >

This is a no-op.

=head2 B< $d-E<gt>msg( ) >

This is a no-op.

=head2 B< $d = inflateInit( [OPTS] ) >

Alias for bzinflateInit, with return code translation.

All OPTS are ignored.

=head2 B< $d-E<gt>inflate( ) >

Alias for bzinflate, with return code translation.

=head2 B< $d-E<gt>inflateSync( ) >

This is a no-op.

=head2 B< $d-E<gt>adler32( $crc ) >

This is a no-op.

=head2 B< $d-E<gt>crc32( $crc ) >

This is a no-op.

=head2 B< $buffer = memGzip( $buffer ) >

Alias for memBzip.

=head2 B< $buffer = memGunzip( $buffer ) >

Alias for memBunzip.

=back

=head1 IN-MEMORY COMPRESS/UNCOMPRESS

Two high-level functions are provided by I<bzlib> to perform in-memory
compression. They are B<memBzip> and B<memBunzip>. Two Perl subs are
provided which provide similar functionality.

=over 5

=head2 B<$compressed = memBzip($buffer);>

Compresses B<$source>. If successful it returns the compressed
data. Otherwise it returns I<undef>.

The buffer parameter can either be a scalar or a scalar reference.

Essentially, an in-memory bzip file is created. It creates a minimal
bzip header.

=head2 B<$uncompressed = memBunzip($buffer);>

Uncompresses B<$source>. If successful it returns the uncompressed
data. Otherwise it returns I<undef>.

The source buffer can either be a scalar or a scalar reference.

The buffer parameter can either be a scalar or a scalar reference. The
contents of the buffer parameter are destroyed after calling this
function.

=back

=head1 STREAM DEFLATE 

The Perl interface will I<always> consume the complete input buffer
before returning. Also the output buffer returned will be
automatically grown to fit the amount of output available.

Here is a definition of the interface available:

=head2 B<($d, $status) = bzdeflateInit( [PARAMS] )>

Initialises a deflation stream. 

If successful, it will return the initialised deflation stream, B<$d>
and B<$status> of C<BZ_OK> in a list context. In scalar context it
returns the deflation stream, B<$d>, only.

If not successful, the returned deflation stream (B<$d>) will be
I<undef> and B<$status> will hold the exact I<bzip2> error code.

The function optionally takes a number of named options specified as
C<-Name=E<gt>value> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.

Here is a list of the valid options:

=over 5

=item B<-verbosity>

Defines the verbosity level. Valid values are 0 through 4,

The default is C<-verbosity =E<gt> 0>.

=item B<-blockSize100k>

Defines the buffering factor of compression method.  The algorithm
buffers all data until the buffer is full, then it flushes all the
data out.  Use -blockSize100k to specify the size of the buffer.

Valid settings are 1 through 9, representing a blocking in multiples
of 100k.

Note that each such block has an overhead of leading and trailing
synchronization bytes.  bzip2 recovery uses this information to
pull useable data out of a corrupted file.

A streaming application would probably want to set the blocking low.

=item B<-workFactor>

The workFactor setting tells the deflation algorithm how much work
to invest to compensate for repetitive data.

workFactor may be a number from 0 to 250 inclusive.  The default setting
is 30.

See the bzip documentation for more information.

=back

Here is an example of using the B<deflateInit> optional parameter list
to override the default buffer size and compression level. All other
options will take their default values.

    bzdeflateInit( -blockSize100k => 1, -verbosity => 1 );

=head2 B<($out, $status) = $d-E<gt>bzdeflate($buffer)>

Deflates the contents of B<$buffer>. The buffer can either be a scalar
or a scalar reference.  When finished, B<$buffer> will be
completely processed (assuming there were no errors). If the deflation
was successful it returns deflated output, B<$out>, and a status
value, B<$status>, of C<Z_OK>.

On error, B<$out> will be I<undef> and B<$status> will contain the
I<zlib> error code.

In a scalar context B<bzdeflate> will return B<$out> only.

As with the internal buffering of the I<deflate> function in I<bzip2>,
it is not necessarily the case that any output will be produced by
this method. So don't rely on the fact that B<$out> is empty for an
error test.  In fact, given the size of bzdeflates internal buffer,
with most files it's likely you won't see any output at all until
flush or close.

=head2 B<($out, $status) = $d-E<gt>bzflush([flush_type])>

Typically used to finish the deflation. Any pending output will be
returned via B<$out>.  B<$status> will have a value C<BZ_OK> if
successful.

In a scalar context B<bzflush> will return B<$out> only.

Note that flushing can seriously degrade the compression ratio, so it
should only be used to terminate a decompression (using C<BZ_FLUSH>) or
when you want to create a I<full flush point> (using C<BZ_FINISH>).

The allowable values for C<flush_type> are C<BZ_FLUSH> and C<BZ_FINISH>.

For a handle opened for "w" (bzwrite), the default is C<BZ_FLUSH>.
For a stream, the default for C<flush_type> is C<BZ_FINISH> (which is
essentially a close and reopen).

It is strongly recommended that you only set the C<flush_type>
parameter if you fully understand the implications of what it
does. See the C<bzip2> documentation for details.

=head2 Example

Here is a trivial example of using B<bzdeflate>. It simply reads standard
input, deflates it and writes it to standard output.

    use strict ;
    use warnings ;

    use Compress::Bzip2 ;

    binmode STDIN;
    binmode STDOUT;
    my $x = bzdeflateInit()
       or die "Cannot create a deflation stream\n" ;

    my ($output, $status) ;
    while (<>)
    {
        ($output, $status) = $x->bzdeflate($_) ;
    
        $status == BZ_OK
            or die "deflation failed\n" ;
    
        print $output ;
    }
    
    ($output, $status) = $x->bzclose() ;
    
    $status == BZ_OK
        or die "deflation failed\n" ;
    
    print $output ;

=head1 STREAM INFLATE

Here is a definition of the interface:

=head2 B<($i, $status) = inflateInit()>

Initialises an inflation stream. 

In a list context it returns the inflation stream, B<$i>, and the
I<zlib> status code (B<$status>). In a scalar context it returns the
inflation stream only.

If successful, B<$i> will hold the inflation stream and B<$status> will
be C<BZ_OK>.

If not successful, B<$i> will be I<undef> and B<$status> will hold the
I<bzlib.h> error code.

The function optionally takes a number of named options specified as
C<-Name=E<gt>value> pairs. This allows individual options to be
tailored without having to specify them all in the parameter list.
 
For backward compatibility, it is also possible to pass the parameters
as a reference to a hash containing the name=>value pairs.
 
The function takes one optional parameter, a reference to a hash.  The
contents of the hash allow the deflation interface to be tailored.
 
Here is a list of the valid options:

=over 5

=item B<-small>

B<small> may be 0 or 1.  Set C<small> to one to use a slower, less
memory intensive algorithm.

=item B<-verbosity>

Defines the verbosity level. Valid values are 0 through 4,

The default is C<-verbosity =E<gt> 0>.

=back

Here is an example of using the B<bzinflateInit> optional parameter.

    bzinflateInit( -small => 1, -verbosity => 1 );

=head2 B<($out, $status) = $i-E<gt>bzinflate($buffer)>

Inflates the complete contents of B<$buffer>. The buffer can either be
a scalar or a scalar reference.

Returns C<BZ_OK> if successful and C<BZ_STREAM_END> if the end of the
compressed data has been successfully reached.  If not successful,
B<$out> will be I<undef> and B<$status> will hold the I<bzlib> error
code.

The C<$buffer> parameter is modified by C<bzinflate>. On completion it
will contain what remains of the input buffer after inflation. This
means that C<$buffer> will be an empty string when the return status
is C<BZ_OK>. When the return status is C<BZ_STREAM_END> the C<$buffer>
parameter will contains what (if anything) was stored in the input
buffer after the deflated data stream.

This feature is useful when processing a file format that encapsulates
a compressed data stream.

=head2 Example

Here is an example of using B<bzinflate>.

    use strict ;
    use warnings ;
    
    use Compress::Bzip2;
    
    my $x = bzinflateInit()
       or die "Cannot create a inflation stream\n" ;
    
    my $input = '' ;
    binmode STDIN;
    binmode STDOUT;
    
    my ($output, $status) ;
    while (read(STDIN, $input, 4096))
    {
        ($output, $status) = $x->bzinflate(\$input) ;
    
        print $output 
            if $status == BZ_OK or $status == BZ_STREAM_END ;
    
        last if $status != BZ_OK ;
    }
    
    die "inflation failed\n"
        unless $status == BZ_STREAM_END ;

=head1 EXAMPLES

Here are some example scripts of using the interface.

=over 5

=head2 B<A bzcat function>

  use strict ;
  use warnings ;
    
  use Compress::Bzip2 ;
    
  die "Usage: bzcat file...\n" unless @ARGV ;
    
  my $file ;
    
  foreach $file (@ARGV) {
    my $buffer ;
    
    my $bz = bzopen($file, "rb") 
       or die "Cannot open $file: $bzerrno\n" ;
    
    print $buffer while $bz->bzread($buffer) > 0 ;
    
    die "Error reading from $file: $bzerrno" . ($bzerrno+0) . "\n" 
       if $bzerrno != BZ_STREAM_END ;
        
    $bz->bzclose() ;
  }

=head2 B<A grep using bzreadline>

  use strict ;
  use warnings ;
    
  use Compress::Bzip2 ;
    
  die "Usage: bzgrep pattern file...\n" unless @ARGV >= 2;
    
  my $pattern = shift ;
    
  my $file ;
    
  foreach $file (@ARGV) {
    my $bz = bzopen($file, "rb") 
       or die "Cannot open $file: $bzerrno\n" ;
    
    while ($bz->bzreadline($_) > 0) {
      print if /$pattern/ ;
    }
    
    die "Error reading from $file: $bzerrno\n" 
      if $bzerrno != Z_STREAM_END ;
        
    $bz->bzclose() ;
  }

=head2 B<Streaming Compression>

This script, I<bzstream>, does the opposite of the I<bzcat> script
above. It reads from standard input and writes a bzip file to standard
output.

  use strict ;
  use warnings ;
    
  use Compress::Bzip2 ;
    
  binmode STDOUT;	# bzopen only sets it on the fd
    
  my $bz = bzopen(\*STDOUT, "wb")
     or die "Cannot open stdout: $bzerrno\n" ;
    
  while (<>) {
    $bz->bzwrite($_) or die "error writing: $bzerrno\n" ;
  }

  $bz->bzclose ;

=back

=head1 EXPORT

Use the tags :all, :utilities, :constants, :bzip1 and :gzip.

=head2 Export tag :all

This exports all the exportable methods.

=head2 Export tag :constants

This exports only the BZ_* constants.

=head2 Export tag :bzip1

This exports the Compress::Bzip2 1.x functions, for compatibility.

   compress
   decompress
   compress_init
   decompress_init
   version

These are actually aliases to memBzip and memBunzip.

=head2 Export tag :utilities

This gives an interface to the bzip2 methods.

    bzopen
    bzinflateInit
    bzdeflateInit
    memBzip
    memBunzip
    bzip2
    bunzip2
    bzcat
    bzlibversion
    $bzerrno

=head2 Export tag :gzip

This gives compatibility with Compress::Zlib.

    gzopen
    gzinflateInit
    gzdeflateInit
    memGzip
    memGunzip
    $gzerrno

=head1 Exportable constants

All the I<bzlib> constants are automatically imported when you make use
of I<Compress::Bzip2>.

  BZ_CONFIG_ERROR
  BZ_DATA_ERROR
  BZ_DATA_ERROR_MAGIC
  BZ_FINISH
  BZ_FINISH_OK
  BZ_FLUSH
  BZ_FLUSH_OK
  BZ_IO_ERROR
  BZ_MAX_UNUSED
  BZ_MEM_ERROR
  BZ_OK
  BZ_OUTBUFF_FULL
  BZ_PARAM_ERROR
  BZ_RUN
  BZ_RUN_OK
  BZ_SEQUENCE_ERROR
  BZ_STREAM_END
  BZ_UNEXPECTED_EOF

=head1 SEE ALSO

The documentation for zlib, bzip2 and Compress::Zlib.

=head1 AUTHOR

Rob Janes, E<lt>arjay at cpan.orgE<gt>

=head1 COPYRIGHT AND LICENSE

Copyright (C) 2005 by Rob Janes

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself, either Perl version 5.8.3 or,
at your option, any later version of Perl 5 you may have available.

=head1 AUTHOR

The I<Compress::Bzip2> module was originally written by Gawdi Azem
F<azemgi@rupert.informatik.uni-stuttgart.de>.

The first I<Compress::Bzip2> module was written by Gawdi Azem
F<azemgi@rupert.informatik.uni-stuttgart.de>.  It provided an
interface to the in memory inflate and deflate routines.

I<Compress::Bzip2> was subsequently passed on to Marco Carnut
F<kiko@tempest.com.br> who shepharded it through to version 1.03, a
set of changes which included upgrades to handle bzlib 1.0.2, and
improvements to the in memory inflate and deflate routines.  The
streaming interface and error information were added by David Robins
F<dbrobins@davidrobins.net>.

Version 2 of I<Compress::Bzip2> is due to Rob Janes, of
arjay@cpan.org.  This release is intended to give an interface
close to that of Compress::Zlib.  It's development forks from 1.00,
not 1.03, so the streaming interface is not the same as that in 1.03,
although apparently compatible as it passes the 1.03 test suite.

=head1 MODIFICATION HISTORY

See the Changes file.

2.00 Second public release of I<Compress::Bzip2>.



