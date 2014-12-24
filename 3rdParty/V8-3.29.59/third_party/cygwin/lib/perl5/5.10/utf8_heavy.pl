package utf8;
use strict;
use warnings;

sub DEBUG () { 0 }

sub DESTROY {}

my %Cache;

our (%PropertyAlias, %PA_reverse, %PropValueAlias, %PVA_reverse, %PVA_abbr_map);

sub croak { require Carp; Carp::croak(@_) }

##
## "SWASH" == "SWATCH HASH". A "swatch" is a swatch of the Unicode landscape.
## It's a data structure that encodes a set of Unicode characters.
##

sub SWASHNEW {
    my ($class, $type, $list, $minbits, $none) = @_;
    local $^D = 0 if $^D;

    print STDERR "SWASHNEW @_\n" if DEBUG;

    ##
    ## Get the list of codepoints for the type.
    ## Called from swash_init (see utf8.c) or SWASHNEW itself.
    ##
    ## Callers of swash_init:
    ##     op.c:pmtrans             -- for tr/// and y///
    ##     regexec.c:regclass_swash -- for /[]/, \p, and \P
    ##     utf8.c:is_utf8_common    -- for common Unicode properties
    ##     utf8.c:to_utf8_case      -- for lc, uc, ucfirst, etc. and //i
    ##
    ## Given a $type, our goal is to fill $list with the set of codepoint
    ## ranges. If $type is false, $list passed is used.
    ##
    ## $minbits:
    ##     For binary properties, $minbits must be 1.
    ##     For character mappings (case and transliteration), $minbits must
    ##     be a number except 1.
    ##
    ## $list (or that filled according to $type):
    ##     Refer to perlunicode.pod, "User-Defined Character Properties."
    ##     
    ##     For binary properties, only characters with the property value
    ##     of True should be listed. The 3rd column, if any, will be ignored.
    ##
    ## To make the parsing of $type clear, this code takes the a rather
    ## unorthodox approach of last'ing out of the block once we have the
    ## info we need. Were this to be a subroutine, the 'last' would just
    ## be a 'return'.
    ##
    my $file; ## file to load data from, and also part of the %Cache key.
    my $ListSorted = 0;

    if ($type)
    {
        $type =~ s/^\s+//;
        $type =~ s/\s+$//;

        print STDERR "type = $type\n" if DEBUG;

      GETFILE:
        {
	    ##
	    ## It could be a user-defined property.
	    ##

	    my $caller1 = $type =~ s/(.+)::// ? $1 : caller(1);

	    if (defined $caller1 && $type =~ /^(?:\w+)$/) {
		my $prop = "${caller1}::$type";
		if (exists &{$prop}) {
		    no strict 'refs';
		    
		    $list = &{$prop};
		    last GETFILE;
		}
	    }

            my $wasIs;

            ($wasIs = $type =~ s/^Is(?:\s+|[-_])?//i)
              or
            $type =~ s/^(?:(?:General(?:\s+|_)?)?Category|gc)\s*[:=]\s*//i
              or
            $type =~ s/^(?:Script|sc)\s*[:=]\s*//i
              or
            $type =~ s/^Block\s*[:=]\s*/In/i;


	    ##
	    ## See if it's in some enumeration.
	    ##
	    require "unicore/PVA.pl";
	    if ($type =~ /^([\w\s]+)[:=]\s*(.*)/) {
		my ($enum, $val) = (lc $1, lc $2);
		$enum =~ tr/ _-//d;
		$val =~ tr/ _-//d;

		my $pa = $PropertyAlias{$enum} ? $enum : $PA_reverse{$enum};
		my $f = $PropValueAlias{$pa}{$val} ? $val : $PVA_reverse{$pa}{lc $val};

		if ($pa and $f) {
		    $pa = "gc_sc" if $pa eq "gc" or $pa eq "sc";
		    $file = "unicore/lib/$pa/$PVA_abbr_map{$pa}{lc $f}.pl";
		    last GETFILE;
		}
	    }
	    else {
		my $t = lc $type;
		$t =~ tr/ _-//d;

		if ($PropValueAlias{gc}{$t} or $PropValueAlias{sc}{$t}) {
		    $file = "unicore/lib/gc_sc/$PVA_abbr_map{gc_sc}{$t}.pl";
		    last GETFILE;
		}
	    }

            ##
            ## See if it's in the direct mapping table.
            ##
            require "unicore/Exact.pl";
            if (my $base = $utf8::Exact{$type}) {
                $file = "unicore/lib/gc_sc/$base.pl";
                last GETFILE;
            }

            ##
            ## If not there exactly, try the canonical form. The canonical
            ## form is lowercased, with any separators (\s+|[-_]) removed.
            ##
            my $canonical = lc $type;
            $canonical =~ s/(?<=[a-z\d])(?:\s+|[-_])(?=[a-z\d])//g;
            print STDERR "canonical = $canonical\n" if DEBUG;

            require "unicore/Canonical.pl";
            if (my $base = ($utf8::Canonical{$canonical} || $utf8::Canonical{ lc $utf8::PropertyAlias{$canonical} })) {
                $file = "unicore/lib/gc_sc/$base.pl";
                last GETFILE;
            }

	    ##
	    ## See if it's a user-level "To".
	    ##

	    my $caller0 = caller(0);

	    if (defined $caller0 && $type =~ /^To(?:\w+)$/) {
		my $map = $caller0 . "::" . $type;

		if (exists &{$map}) {
		    no strict 'refs';
		    
		    $list = &{$map};
		    last GETFILE;
		}
	    }

            ##
            ## Last attempt -- see if it's a standard "To" name
	    ## (e.g. "ToLower")  ToTitle is used by ucfirst().
	    ## The user-level way to access ToDigit() and ToFold()
	    ## is to use Unicode::UCD.
            ##
            if ($type =~ /^To(Digit|Fold|Lower|Title|Upper)$/) {
                $file = "unicore/To/$1.pl";
                ## would like to test to see if $file actually exists....
                last GETFILE;
            }

            ##
            ## If we reach this line, it's because we couldn't figure
            ## out what to do with $type. Ouch.
            ##

            return $type;
        }

	if (defined $file) {
	    print STDERR "found it (file='$file')\n" if DEBUG;

	    ##
	    ## If we reach here, it was due to a 'last GETFILE' above
	    ## (exception: user-defined properties and mappings), so we
	    ## have a filename, so now we load it if we haven't already.
	    ## If we have, return the cached results. The cache key is the
	    ## class and file to load.
	    ##
	    my $found = $Cache{$class, $file};
	    if ($found and ref($found) eq $class) {
		print STDERR "Returning cached '$file' for \\p{$type}\n" if DEBUG;
		return $found;
	    }

	    $list = do $file; die $@ if $@;
	}

        $ListSorted = 1; ## we know that these lists are sorted
    }

    my $extras;
    my $bits = $minbits;

    my $ORIG = $list;
    if ($list) {
	my @tmp = split(/^/m, $list);
	my %seen;
	no warnings;
	$extras = join '', grep /^[^0-9a-fA-F]/, @tmp;
	$list = join '',
	    map  { $_->[1] }
	    sort { $a->[0] <=> $b->[0] }
	    map  { /^([0-9a-fA-F]+)/; [ CORE::hex($1), $_ ] }
	    grep { /^([0-9a-fA-F]+)/ and not $seen{$1}++ } @tmp; # XXX doesn't do ranges right
    }

    if ($none) {
	my $hextra = sprintf "%04x", $none + 1;
	$list =~ s/\tXXXX$/\t$hextra/mg;
    }

    if ($minbits != 1 && $minbits < 32) { # not binary property
	my $top = 0;
	while ($list =~ /^([0-9a-fA-F]+)(?:[\t]([0-9a-fA-F]+)?)(?:[ \t]([0-9a-fA-F]+))?/mg) {
	    my $min = CORE::hex $1;
	    my $max = defined $2 ? CORE::hex $2 : $min;
	    my $val = defined $3 ? CORE::hex $3 : 0;
	    $val += $max - $min if defined $3;
	    $top = $val if $val > $top;
	}
	my $topbits =
	    $top > 0xffff ? 32 :
	    $top > 0xff ? 16 : 8;
	$bits = $topbits if $bits < $topbits;
    }

    my @extras;
    for my $x ($extras) {
	pos $x = 0;
	while ($x =~ /^([^0-9a-fA-F\n])(.*)/mg) {
	    my $char = $1;
	    my $name = $2;
	    print STDERR "$1 => $2\n" if DEBUG;
	    if ($char =~ /[-+!&]/) {
		my ($c,$t) = split(/::/, $name, 2);	# bogus use of ::, really
		my $subobj;
		if ($c eq 'utf8') {
		    $subobj = utf8->SWASHNEW($t, "", $minbits, 0);
		}
		elsif (exists &$name) {
		    $subobj = utf8->SWASHNEW($name, "", $minbits, 0);
		}
		elsif ($c =~ /^([0-9a-fA-F]+)/) {
		    $subobj = utf8->SWASHNEW("", $c, $minbits, 0);
		}
		return $subobj unless ref $subobj;
		push @extras, $name => $subobj;
		$bits = $subobj->{BITS} if $bits < $subobj->{BITS};
	    }
	}
    }

    print STDERR "CLASS = $class, TYPE => $type, BITS => $bits, NONE => $none\nEXTRAS =>\n$extras\nLIST =>\n$list\n" if DEBUG;

    my $SWASH = bless {
	TYPE => $type,
	BITS => $bits,
	EXTRAS => $extras,
	LIST => $list,
	NONE => $none,
	@extras,
    } => $class;

    if ($file) {
        $Cache{$class, $file} = $SWASH;
    }

    return $SWASH;
}

# Now SWASHGET is recasted into a C function S_swash_get (see utf8.c).

1;
