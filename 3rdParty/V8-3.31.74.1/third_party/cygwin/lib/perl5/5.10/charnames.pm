package charnames;
use strict;
use warnings;
use File::Spec;
our $VERSION = '1.06';

use bytes ();		# for $bytes::hint_bits

my %alias1 = (
		# Icky 3.2 names with parentheses.
		'LINE FEED'		=> 'LINE FEED (LF)',
		'FORM FEED'		=> 'FORM FEED (FF)',
		'CARRIAGE RETURN'	=> 'CARRIAGE RETURN (CR)',
		'NEXT LINE'		=> 'NEXT LINE (NEL)',
		# Convenience.
		'LF'			=> 'LINE FEED (LF)',
		'FF'			=> 'FORM FEED (FF)',
		'CR'			=> 'CARRIAGE RETURN (CR)',
		'NEL'			=> 'NEXT LINE (NEL)',
	        # More convenience.  For futher convencience,
	        # it is suggested some way using using the NamesList
		# aliases is implemented.
	        'ZWNJ'			=> 'ZERO WIDTH NON-JOINER',
	        'ZWJ'			=> 'ZERO WIDTH JOINER',
		'BOM'			=> 'BYTE ORDER MARK',
	    );

my %alias2 = (
		# Pre-3.2 compatibility (only for the first 256 characters).
		'HORIZONTAL TABULATION'	=> 'CHARACTER TABULATION',
		'VERTICAL TABULATION'	=> 'LINE TABULATION',
		'FILE SEPARATOR'	=> 'INFORMATION SEPARATOR FOUR',
		'GROUP SEPARATOR'	=> 'INFORMATION SEPARATOR THREE',
		'RECORD SEPARATOR'	=> 'INFORMATION SEPARATOR TWO',
		'UNIT SEPARATOR'	=> 'INFORMATION SEPARATOR ONE',
		'PARTIAL LINE DOWN'	=> 'PARTIAL LINE FORWARD',
		'PARTIAL LINE UP'	=> 'PARTIAL LINE BACKWARD',
	    );

my %alias3 = (
		# User defined aliasses. Even more convenient :)
	    );
my $txt;

sub croak
{
  require Carp; goto &Carp::croak;
} # croak

sub carp
{
  require Carp; goto &Carp::carp;
} # carp

sub alias (@)
{
  @_ or return %alias3;
  my $alias = ref $_[0] ? $_[0] : { @_ };
  @alias3{keys %$alias} = values %$alias;
} # alias

sub alias_file ($)
{
  my ($arg, $file) = @_;
  if (-f $arg && File::Spec->file_name_is_absolute ($arg)) {
    $file = $arg;
  }
  elsif ($arg =~ m/^\w+$/) {
    $file = "unicore/${arg}_alias.pl";
  }
  else {
    croak "Charnames alias files can only have identifier characters";
  }
  if (my @alias = do $file) {
    @alias == 1 && !defined $alias[0] and
      croak "$file cannot be used as alias file for charnames";
    @alias % 2 and
      croak "$file did not return a (valid) list of alias pairs";
    alias (@alias);
    return (1);
  }
  0;
} # alias_file

# This is not optimized in any way yet
sub charnames
{
  my $name = shift;

  if (exists $alias1{$name}) {
    $name = $alias1{$name};
  }
  elsif (exists $alias2{$name}) {
    require warnings;
    warnings::warnif('deprecated', qq{Unicode character name "$name" is deprecated, use "$alias2{$name}" instead});
    $name = $alias2{$name};
  }
  elsif (exists $alias3{$name}) {
    $name = $alias3{$name};
  }

  my $ord;
  my @off;
  my $fname;

  if ($name eq "BYTE ORDER MARK") {
    $fname = $name;
    $ord = 0xFEFF;
  } else {
    ## Suck in the code/name list as a big string.
    ## Lines look like:
    ##     "0052\t\tLATIN CAPITAL LETTER R\n"
    $txt = do "unicore/Name.pl" unless $txt;

    ## @off will hold the index into the code/name string of the start and
    ## end of the name as we find it.

    ## If :full, look for the name exactly
    if ($^H{charnames_full} and $txt =~ /\t\t\Q$name\E$/m) {
      @off = ($-[0], $+[0]);
    }

    ## If we didn't get above, and :short allowed, look for the short name.
    ## The short name is like "greek:Sigma"
    unless (@off) {
      if ($^H{charnames_short} and $name =~ /^(.+?):(.+)/s) {
	my ($script, $cname) = ($1, $2);
	my $case = $cname =~ /[[:upper:]]/ ? "CAPITAL" : "SMALL";
	if ($txt =~ m/\t\t\U$script\E (?:$case )?LETTER \U\Q$cname\E$/m) {
	  @off = ($-[0], $+[0]);
	}
      }
    }

    ## If we still don't have it, check for the name among the loaded
    ## scripts.
    if (not @off) {
      my $case = $name =~ /[[:upper:]]/ ? "CAPITAL" : "SMALL";
      for my $script (@{$^H{charnames_scripts}}) {
	if ($txt =~ m/\t\t$script (?:$case )?LETTER \U\Q$name\E$/m) {
	  @off = ($-[0], $+[0]);
	  last;
	}
      }
    }

    ## If we don't have it by now, give up.
    unless (@off) {
      carp "Unknown charname '$name'";
      return "\x{FFFD}";
    }

    ##
    ## Now know where in the string the name starts.
    ## The code, in hex, is before that.
    ##
    ## The code can be 4-6 characters long, so we've got to sort of
    ## go look for it, just after the newline that comes before $off[0].
    ##
    ## This would be much easier if unicore/Name.pl had info in
    ## a name/code order, instead of code/name order.
    ##
    ## The +1 after the rindex() is to skip past the newline we're finding,
    ## or, if the rindex() fails, to put us to an offset of zero.
    ##
    my $hexstart = rindex($txt, "\n", $off[0]) + 1;

    ## we know where it starts, so turn into number -
    ## the ordinal for the char.
    $ord = CORE::hex substr($txt, $hexstart, $off[0] - $hexstart);
  }

  if ($^H & $bytes::hint_bits) {	# "use bytes" in effect?
    use bytes;
    return chr $ord if $ord <= 255;
    my $hex = sprintf "%04x", $ord;
    if (not defined $fname) {
      $fname = substr $txt, $off[0] + 2, $off[1] - $off[0] - 2;
    }
    croak "Character 0x$hex with name '$fname' is above 0xFF";
  }

  no warnings 'utf8'; # allow even illegal characters
  return pack "U", $ord;
} # charnames

sub import
{
  shift; ## ignore class name

  if (not @_) {
    carp("`use charnames' needs explicit imports list");
  }
  $^H{charnames} = \&charnames ;

  ##
  ## fill %h keys with our @_ args.
  ##
  my ($promote, %h, @args) = (0);
  while (my $arg = shift) {
    if ($arg eq ":alias") {
      @_ or
	croak ":alias needs an argument in charnames";
      my $alias = shift;
      if (ref $alias) {
	ref $alias eq "HASH" or
	  croak "Only HASH reference supported as argument to :alias";
	alias ($alias);
	next;
      }
      if ($alias =~ m{:(\w+)$}) {
	$1 eq "full" || $1 eq "short" and
	  croak ":alias cannot use existing pragma :$1 (reversed order?)";
	alias_file ($1) and $promote = 1;
	next;
      }
      alias_file ($alias);
      next;
    }
    if (substr($arg, 0, 1) eq ':' and ! ($arg eq ":full" || $arg eq ":short")) {
      warn "unsupported special '$arg' in charnames";
      next;
    }
    push @args, $arg;
  }
  @args == 0 && $promote and @args = (":full");
  @h{@args} = (1) x @args;

  $^H{charnames_full} = delete $h{':full'};
  $^H{charnames_short} = delete $h{':short'};
  $^H{charnames_scripts} = [map uc, keys %h];

  ##
  ## If utf8? warnings are enabled, and some scripts were given,
  ## see if at least we can find one letter of each script.
  ##
  if (warnings::enabled('utf8') && @{$^H{charnames_scripts}}) {
    $txt = do "unicore/Name.pl" unless $txt;

    for my $script (@{$^H{charnames_scripts}}) {
      if (not $txt =~ m/\t\t$script (?:CAPITAL |SMALL )?LETTER /) {
	warnings::warn('utf8',  "No such script: '$script'");
      }
    }
  }
} # import

my %viacode;

sub viacode
{
  if (@_ != 1) {
    carp "charnames::viacode() expects one argument";
    return;
  }

  my $arg = shift;

  # this comes actually from Unicode::UCD, where it is the named
  # function _getcode (), but it avoids the overhead of loading it
  my $hex;
  if ($arg =~ /^[1-9]\d*$/) {
    $hex = sprintf "%04X", $arg;
  } elsif ($arg =~ /^(?:[Uu]\+|0[xX])?([[:xdigit:]]+)$/) {
    $hex = $1;
  } else {
    carp("unexpected arg \"$arg\" to charnames::viacode()");
    return;
  }

  # checking the length first is slightly faster
  if (length($hex) > 5 && hex($hex) > 0x10FFFF) {
    carp "Unicode characters only allocated up to U+10FFFF (you asked for U+$hex)";
    return;
  }

  return $viacode{$hex} if exists $viacode{$hex};

  $txt = do "unicore/Name.pl" unless $txt;

  return unless $txt =~ m/^$hex\t\t(.+)/m;

  $viacode{$hex} = $1;
} # viacode

my %vianame;

sub vianame
{
  if (@_ != 1) {
    carp "charnames::vianame() expects one name argument";
    return ()
  }

  my $arg = shift;

  return chr CORE::hex $1 if $arg =~ /^U\+([0-9a-fA-F]+)$/;

  return $vianame{$arg} if exists $vianame{$arg};

  $txt = do "unicore/Name.pl" unless $txt;

  my $pos = index $txt, "\t\t$arg\n";
  if ($[ <= $pos) {
    my $posLF = rindex $txt, "\n", $pos;
    (my $code = substr $txt, $posLF + 1, 6) =~ tr/\t//d;
    return $vianame{$arg} = CORE::hex $code;

    # If $pos is at the 1st line, $posLF must be $[ - 1 (not found);
    # then $posLF + 1 equals to $[ (at the beginning of $txt).
    # Otherwise $posLF is the position of "\n";
    # then $posLF + 1 must be the position of the next to "\n"
    # (the beginning of the line).
    # substr($txt, $posLF + 1, 6) may be "0000\t\t", "00A1\t\t",
    # "10300\t", "100000", etc. So we can get the code via removing TAB.
  } else {
    return;
  }
} # vianame


1;
__END__

=head1 NAME

charnames - define character names for C<\N{named}> string literal escapes

=head1 SYNOPSIS

  use charnames ':full';
  print "\N{GREEK SMALL LETTER SIGMA} is called sigma.\n";

  use charnames ':short';
  print "\N{greek:Sigma} is an upper-case sigma.\n";

  use charnames qw(cyrillic greek);
  print "\N{sigma} is Greek sigma, and \N{be} is Cyrillic b.\n";

  use charnames ":full", ":alias" => {
    e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE",
  };
  print "\N{e_ACUTE} is a small letter e with an acute.\n";

  use charnames ();
  print charnames::viacode(0x1234); # prints "ETHIOPIC SYLLABLE SEE"
  printf "%04X", charnames::vianame("GOTHIC LETTER AHSA"); # prints "10330"

=head1 DESCRIPTION

Pragma C<use charnames> supports arguments C<:full>, C<:short>, script
names and customized aliases.  If C<:full> is present, for expansion of
C<\N{CHARNAME}>, the string C<CHARNAME> is first looked up in the list of
standard Unicode character names.  If C<:short> is present, and
C<CHARNAME> has the form C<SCRIPT:CNAME>, then C<CNAME> is looked up
as a letter in script C<SCRIPT>.  If pragma C<use charnames> is used
with script name arguments, then for C<\N{CHARNAME}> the name
C<CHARNAME> is looked up as a letter in the given scripts (in the
specified order). Customized aliases are explained in L</CUSTOM ALIASES>.

For lookup of C<CHARNAME> inside a given script C<SCRIPTNAME>
this pragma looks for the names

  SCRIPTNAME CAPITAL LETTER CHARNAME
  SCRIPTNAME SMALL LETTER CHARNAME
  SCRIPTNAME LETTER CHARNAME

in the table of standard Unicode names.  If C<CHARNAME> is lowercase,
then the C<CAPITAL> variant is ignored, otherwise the C<SMALL> variant
is ignored.

Note that C<\N{...}> is compile-time, it's a special form of string
constant used inside double-quoted strings: in other words, you cannot
use variables inside the C<\N{...}>.  If you want similar run-time
functionality, use charnames::vianame().

For the C0 and C1 control characters (U+0000..U+001F, U+0080..U+009F)
as of Unicode 3.1, there are no official Unicode names but you can use
instead the ISO 6429 names (LINE FEED, ESCAPE, and so forth).  In
Unicode 3.2 (as of Perl 5.8) some naming changes take place ISO 6429
has been updated, see L</ALIASES>.  Also note that the U+UU80, U+0081,
U+0084, and U+0099 do not have names even in ISO 6429.

Since the Unicode standard uses "U+HHHH", so can you: "\N{U+263a}"
is the Unicode smiley face, or "\N{WHITE SMILING FACE}".

=head1 ALIASES

A few aliases have been defined for convenience: instead of having
to use the official names

    LINE FEED (LF)
    FORM FEED (FF)
    CARRIAGE RETURN (CR)
    NEXT LINE (NEL)

(yes, with parentheses) one can use

    LINE FEED
    FORM FEED
    CARRIAGE RETURN
    NEXT LINE
    LF
    FF
    CR
    NEL

One can also use

    BYTE ORDER MARK
    BOM

and

    ZWNJ
    ZWJ

for ZERO WIDTH NON-JOINER and ZERO WIDTH JOINER.

For backward compatibility one can use the old names for
certain C0 and C1 controls

    old                         new

    HORIZONTAL TABULATION       CHARACTER TABULATION
    VERTICAL TABULATION         LINE TABULATION
    FILE SEPARATOR              INFORMATION SEPARATOR FOUR
    GROUP SEPARATOR             INFORMATION SEPARATOR THREE
    RECORD SEPARATOR            INFORMATION SEPARATOR TWO
    UNIT SEPARATOR              INFORMATION SEPARATOR ONE
    PARTIAL LINE DOWN           PARTIAL LINE FORWARD
    PARTIAL LINE UP             PARTIAL LINE BACKWARD

but the old names in addition to giving the character
will also give a warning about being deprecated.

=head1 CUSTOM ALIASES

This version of charnames supports three mechanisms of adding local
or customized aliases to standard Unicode naming conventions (:full)

=head2 Anonymous hashes

    use charnames ":full", ":alias" => {
        e_ACUTE => "LATIN SMALL LETTER E WITH ACUTE",
        };
    my $str = "\N{e_ACUTE}";

=head2 Alias file

    use charnames ":full", ":alias" => "pro";

    will try to read "unicore/pro_alias.pl" from the @INC path. This
    file should return a list in plain perl:

    (
    A_GRAVE         => "LATIN CAPITAL LETTER A WITH GRAVE",
    A_CIRCUM        => "LATIN CAPITAL LETTER A WITH CIRCUMFLEX",
    A_DIAERES       => "LATIN CAPITAL LETTER A WITH DIAERESIS",
    A_TILDE         => "LATIN CAPITAL LETTER A WITH TILDE",
    A_BREVE         => "LATIN CAPITAL LETTER A WITH BREVE",
    A_RING          => "LATIN CAPITAL LETTER A WITH RING ABOVE",
    A_MACRON        => "LATIN CAPITAL LETTER A WITH MACRON",
    );

=head2 Alias shortcut

    use charnames ":alias" => ":pro";

    works exactly the same as the alias pairs, only this time,
    ":full" is inserted automatically as first argument (if no
    other argument is given).

=head1 charnames::viacode(code)

Returns the full name of the character indicated by the numeric code.
The example

    print charnames::viacode(0x2722);

prints "FOUR TEARDROP-SPOKED ASTERISK".

Returns undef if no name is known for the code.

This works only for the standard names, and does not yet apply
to custom translators.

Notice that the name returned for of U+FEFF is "ZERO WIDTH NO-BREAK
SPACE", not "BYTE ORDER MARK".

=head1 charnames::vianame(name)

Returns the code point indicated by the name.
The example

    printf "%04X", charnames::vianame("FOUR TEARDROP-SPOKED ASTERISK");

prints "2722".

Returns undef if the name is unknown.

This works only for the standard names, and does not yet apply
to custom translators.

=head1 CUSTOM TRANSLATORS

The mechanism of translation of C<\N{...}> escapes is general and not
hardwired into F<charnames.pm>.  A module can install custom
translations (inside the scope which C<use>s the module) with the
following magic incantation:

    sub import {
	shift;
	$^H{charnames} = \&translator;
    }

Here translator() is a subroutine which takes C<CHARNAME> as an
argument, and returns text to insert into the string instead of the
C<\N{CHARNAME}> escape.  Since the text to insert should be different
in C<bytes> mode and out of it, the function should check the current
state of C<bytes>-flag as in:

    use bytes ();			# for $bytes::hint_bits
    sub translator {
	if ($^H & $bytes::hint_bits) {
	    return bytes_translator(@_);
	}
	else {
	    return utf8_translator(@_);
	}
    }

=head1 ILLEGAL CHARACTERS

If you ask by name for a character that does not exist, a warning is
given and the Unicode I<replacement character> "\x{FFFD}" is returned.

If you ask by code for a character that does not exist, no warning is
given and C<undef> is returned.  (Though if you ask for a code point
past U+10FFFF you do get a warning.)

=head1 BUGS

Since evaluation of the translation function happens in a middle of
compilation (of a string literal), the translation function should not
do any C<eval>s or C<require>s.  This restriction should be lifted in
a future version of Perl.

=cut
