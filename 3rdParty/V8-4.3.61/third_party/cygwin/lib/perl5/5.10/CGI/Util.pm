package CGI::Util;

use strict;
use vars qw($VERSION @EXPORT_OK @ISA $EBCDIC @A2E @E2A);
require Exporter;
@ISA = qw(Exporter);
@EXPORT_OK = qw(rearrange make_attributes unescape escape 
		expires ebcdic2ascii ascii2ebcdic);

$VERSION = '1.5_01';

$EBCDIC = "\t" ne "\011";
# (ord('^') == 95) for codepage 1047 as on os390, vmesa
@A2E = (
   0,  1,  2,  3, 55, 45, 46, 47, 22,  5, 21, 11, 12, 13, 14, 15,
  16, 17, 18, 19, 60, 61, 50, 38, 24, 25, 63, 39, 28, 29, 30, 31,
  64, 90,127,123, 91,108, 80,125, 77, 93, 92, 78,107, 96, 75, 97,
 240,241,242,243,244,245,246,247,248,249,122, 94, 76,126,110,111,
 124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
 215,216,217,226,227,228,229,230,231,232,233,173,224,189, 95,109,
 121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
 151,152,153,162,163,164,165,166,167,168,169,192, 79,208,161,  7,
  32, 33, 34, 35, 36, 37,  6, 23, 40, 41, 42, 43, 44,  9, 10, 27,
  48, 49, 26, 51, 52, 53, 54,  8, 56, 57, 58, 59,  4, 20, 62,255,
  65,170, 74,177,159,178,106,181,187,180,154,138,176,202,175,188,
 144,143,234,250,190,160,182,179,157,218,155,139,183,184,185,171,
 100,101, 98,102, 99,103,158,104,116,113,114,115,120,117,118,119,
 172,105,237,238,235,239,236,191,128,253,254,251,252,186,174, 89,
  68, 69, 66, 70, 67, 71,156, 72, 84, 81, 82, 83, 88, 85, 86, 87,
 140, 73,205,206,203,207,204,225,112,221,222,219,220,141,142,223
	 );
@E2A = (
   0,  1,  2,  3,156,  9,134,127,151,141,142, 11, 12, 13, 14, 15,
  16, 17, 18, 19,157, 10,  8,135, 24, 25,146,143, 28, 29, 30, 31,
 128,129,130,131,132,133, 23, 27,136,137,138,139,140,  5,  6,  7,
 144,145, 22,147,148,149,150,  4,152,153,154,155, 20, 21,158, 26,
  32,160,226,228,224,225,227,229,231,241,162, 46, 60, 40, 43,124,
  38,233,234,235,232,237,238,239,236,223, 33, 36, 42, 41, 59, 94,
  45, 47,194,196,192,193,195,197,199,209,166, 44, 37, 95, 62, 63,
 248,201,202,203,200,205,206,207,204, 96, 58, 35, 64, 39, 61, 34,
 216, 97, 98, 99,100,101,102,103,104,105,171,187,240,253,254,177,
 176,106,107,108,109,110,111,112,113,114,170,186,230,184,198,164,
 181,126,115,116,117,118,119,120,121,122,161,191,208, 91,222,174,
 172,163,165,183,169,167,182,188,189,190,221,168,175, 93,180,215,
 123, 65, 66, 67, 68, 69, 70, 71, 72, 73,173,244,246,242,243,245,
 125, 74, 75, 76, 77, 78, 79, 80, 81, 82,185,251,252,249,250,255,
  92,247, 83, 84, 85, 86, 87, 88, 89, 90,178,212,214,210,211,213,
  48, 49, 50, 51, 52, 53, 54, 55, 56, 57,179,219,220,217,218,159
	 );

if ($EBCDIC && ord('^') == 106) { # as in the BS2000 posix-bc coded character set
     $A2E[91] = 187;   $A2E[92] = 188;  $A2E[94] = 106;  $A2E[96] = 74;
     $A2E[123] = 251;  $A2E[125] = 253; $A2E[126] = 255; $A2E[159] = 95;
     $A2E[162] = 176;  $A2E[166] = 208; $A2E[168] = 121; $A2E[172] = 186;
     $A2E[175] = 161;  $A2E[217] = 224; $A2E[219] = 221; $A2E[221] = 173;
     $A2E[249] = 192;

     $E2A[74] = 96;   $E2A[95] = 159;  $E2A[106] = 94;  $E2A[121] = 168;
     $E2A[161] = 175; $E2A[173] = 221; $E2A[176] = 162; $E2A[186] = 172;
     $E2A[187] = 91;  $E2A[188] = 92;  $E2A[192] = 249; $E2A[208] = 166;
     $E2A[221] = 219; $E2A[224] = 217; $E2A[251] = 123; $E2A[253] = 125;
     $E2A[255] = 126;
   }
elsif ($EBCDIC && ord('^') == 176) { # as in codepage 037 on os400
  $A2E[10] = 37;  $A2E[91] = 186;  $A2E[93] = 187; $A2E[94] = 176;
  $A2E[133] = 21; $A2E[168] = 189; $A2E[172] = 95; $A2E[221] = 173;

  $E2A[21] = 133; $E2A[37] = 10;  $E2A[95] = 172; $E2A[173] = 221;
  $E2A[176] = 94; $E2A[186] = 91; $E2A[187] = 93; $E2A[189] = 168;
}

# Smart rearrangement of parameters to allow named parameter
# calling.  We do the rearangement if:
# the first parameter begins with a -
sub rearrange {
    my($order,@param) = @_;
    return () unless @param;

    if (ref($param[0]) eq 'HASH') {
	@param = %{$param[0]};
    } else {
	return @param 
	    unless (defined($param[0]) && substr($param[0],0,1) eq '-');
    }

    # map parameters into positional indices
    my ($i,%pos);
    $i = 0;
    foreach (@$order) {
	foreach (ref($_) eq 'ARRAY' ? @$_ : $_) { $pos{lc($_)} = $i; }
	$i++;
    }

    my (@result,%leftover);
    $#result = $#$order;  # preextend
    while (@param) {
	my $key = lc(shift(@param));
	$key =~ s/^\-//;
	if (exists $pos{$key}) {
	    $result[$pos{$key}] = shift(@param);
	} else {
	    $leftover{$key} = shift(@param);
	}
    }

    push (@result,make_attributes(\%leftover,defined $CGI::Q ? $CGI::Q->{escape} : 1)) if %leftover;
    @result;
}

sub make_attributes {
    my $attr = shift;
    return () unless $attr && ref($attr) && ref($attr) eq 'HASH';
    my $escape =  shift || 0;
    my(@att);
    foreach (keys %{$attr}) {
	my($key) = $_;
	$key=~s/^\-//;     # get rid of initial - if present

	# old way: breaks EBCDIC!
	# $key=~tr/A-Z_/a-z-/; # parameters are lower case, use dashes

	($key="\L$key") =~ tr/_/-/; # parameters are lower case, use dashes

	my $value = $escape ? simple_escape($attr->{$_}) : $attr->{$_};
	push(@att,defined($attr->{$_}) ? qq/$key="$value"/ : qq/$key/);
    }
    return @att;
}

sub simple_escape {
  return unless defined(my $toencode = shift);
  $toencode =~ s{&}{&amp;}gso;
  $toencode =~ s{<}{&lt;}gso;
  $toencode =~ s{>}{&gt;}gso;
  $toencode =~ s{\"}{&quot;}gso;
# Doesn't work.  Can't work.  forget it.
#  $toencode =~ s{\x8b}{&#139;}gso;
#  $toencode =~ s{\x9b}{&#155;}gso;
  $toencode;
}

sub utf8_chr {
        my $c = shift(@_);
	if ($] >= 5.006){
	    require utf8;
	    my $u = chr($c);
	    utf8::encode($u); # drop utf8 flag
	    return $u;
	}
        if ($c < 0x80) {
                return sprintf("%c", $c);
        } elsif ($c < 0x800) {
                return sprintf("%c%c", 0xc0 | ($c >> 6), 0x80 | ($c & 0x3f));
        } elsif ($c < 0x10000) {
                return sprintf("%c%c%c",
                                           0xe0 |  ($c >> 12),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } elsif ($c < 0x200000) {
                return sprintf("%c%c%c%c",
                                           0xf0 |  ($c >> 18),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } elsif ($c < 0x4000000) {
                return sprintf("%c%c%c%c%c",
                                           0xf8 |  ($c >> 24),
                                           0x80 | (($c >> 18) & 0x3f),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >>  6) & 0x3f),
                                           0x80 | ( $c          & 0x3f));

        } elsif ($c < 0x80000000) {
                return sprintf("%c%c%c%c%c%c",
                                           0xfc |  ($c >> 30),
                                           0x80 | (($c >> 24) & 0x3f),
                                           0x80 | (($c >> 18) & 0x3f),
                                           0x80 | (($c >> 12) & 0x3f),
                                           0x80 | (($c >> 6)  & 0x3f),
                                           0x80 | ( $c          & 0x3f));
        } else {
                return utf8_chr(0xfffd);
        }
}

# unescape URL-encoded data
sub unescape {
  shift() if @_ > 0 and (ref($_[0]) || (defined $_[1] && $_[0] eq $CGI::DefaultClass));
  my $todecode = shift;
  return undef unless defined($todecode);
  $todecode =~ tr/+/ /;       # pluses become spaces
    $EBCDIC = "\t" ne "\011";
    if ($EBCDIC) {
      $todecode =~ s/%([0-9a-fA-F]{2})/chr $A2E[hex($1)]/ge;
    } else {
	# handle surrogate pairs first -- dankogai
	$todecode =~ s{
			%u([Dd][89a-bA-B][0-9a-fA-F]{2}) # hi
		        %u([Dd][c-fC-F][0-9a-fA-F]{2})   # lo
		      }{
			  utf8_chr(
				   0x10000 
				   + (hex($1) - 0xD800) * 0x400 
				   + (hex($2) - 0xDC00)
				  )
		      }gex;
      $todecode =~ s/%(?:([0-9a-fA-F]{2})|u([0-9a-fA-F]{4}))/
	defined($1)? chr hex($1) : utf8_chr(hex($2))/ge;
    }
  return $todecode;
}

# URL-encode data
sub escape {
  shift() if @_ > 1 and ( ref($_[0]) || (defined $_[1] && $_[0] eq $CGI::DefaultClass));
  my $toencode = shift;
  return undef unless defined($toencode);
  $toencode = eval { pack("C*", unpack("U0C*", $toencode))} || pack("C*", unpack("C*", $toencode));

  # force bytes while preserving backward compatibility -- dankogai
  # but commented out because it was breaking CGI::Compress -- lstein
  # $toencode = eval { pack("U*", unpack("U0C*", $toencode))} || pack("C*", unpack("C*", $toencode));

    if ($EBCDIC) {
      $toencode=~s/([^a-zA-Z0-9_.~-])/uc sprintf("%%%02x",$E2A[ord($1)])/eg;
    } else {
      $toencode=~s/([^a-zA-Z0-9_.~-])/uc sprintf("%%%02x",ord($1))/eg;
    }
  return $toencode;
}

# This internal routine creates date strings suitable for use in
# cookies and HTTP headers.  (They differ, unfortunately.)
# Thanks to Mark Fisher for this.
sub expires {
    my($time,$format) = @_;
    $format ||= 'http';

    my(@MON)=qw/Jan Feb Mar Apr May Jun Jul Aug Sep Oct Nov Dec/;
    my(@WDAY) = qw/Sun Mon Tue Wed Thu Fri Sat/;

    # pass through preformatted dates for the sake of expire_calc()
    $time = expire_calc($time);
    return $time unless $time =~ /^\d+$/;

    # make HTTP/cookie date string from GMT'ed time
    # (cookies use '-' as date separator, HTTP uses ' ')
    my($sc) = ' ';
    $sc = '-' if $format eq "cookie";
    my($sec,$min,$hour,$mday,$mon,$year,$wday) = gmtime($time);
    $year += 1900;
    return sprintf("%s, %02d$sc%s$sc%04d %02d:%02d:%02d GMT",
                   $WDAY[$wday],$mday,$MON[$mon],$year,$hour,$min,$sec);
}

# This internal routine creates an expires time exactly some number of
# hours from the current time.  It incorporates modifications from 
# Mark Fisher.
sub expire_calc {
    my($time) = @_;
    my(%mult) = ('s'=>1,
                 'm'=>60,
                 'h'=>60*60,
                 'd'=>60*60*24,
                 'M'=>60*60*24*30,
                 'y'=>60*60*24*365);
    # format for time can be in any of the forms...
    # "now" -- expire immediately
    # "+180s" -- in 180 seconds
    # "+2m" -- in 2 minutes
    # "+12h" -- in 12 hours
    # "+1d"  -- in 1 day
    # "+3M"  -- in 3 months
    # "+2y"  -- in 2 years
    # "-3m"  -- 3 minutes ago(!)
    # If you don't supply one of these forms, we assume you are
    # specifying the date yourself
    my($offset);
    if (!$time || (lc($time) eq 'now')) {
      $offset = 0;
    } elsif ($time=~/^\d+/) {
      return $time;
    } elsif ($time=~/^([+-]?(?:\d+|\d*\.\d*))([smhdMy])/) {
      $offset = ($mult{$2} || 1)*$1;
    } else {
      return $time;
    }
    return (time+$offset);
}

sub ebcdic2ascii {
  my $data = shift;
  $data =~ s/(.)/chr $E2A[ord($1)]/ge;
  $data;
}

sub ascii2ebcdic {
  my $data = shift;
  $data =~ s/(.)/chr $A2E[ord($1)]/ge;
  $data;
}

1;

__END__

=head1 NAME

CGI::Util - Internal utilities used by CGI module

=head1 SYNOPSIS

none

=head1 DESCRIPTION

no public subroutines

=head1 AUTHOR INFORMATION

Copyright 1995-1998, Lincoln D. Stein.  All rights reserved.  

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Address bug reports and comments to: lstein@cshl.org.  When sending
bug reports, please provide the version of CGI.pm, the version of
Perl, the name and version of your Web server, and the name and
version of the operating system you are using.  If the problem is even
remotely browser dependent, please provide information about the
affected browers as well.

=head1 SEE ALSO

L<CGI>

=cut
