package Text::ParseWords;

use vars qw($VERSION @ISA @EXPORT $PERL_SINGLE_QUOTE);
$VERSION = "3.26";

require 5.000;

use Exporter;
@ISA = qw(Exporter);
@EXPORT = qw(shellwords quotewords nested_quotewords parse_line);
@EXPORT_OK = qw(old_shellwords);


sub shellwords {
    my (@lines) = @_;
    my @allwords;

    foreach my $line (@lines) {
	$line =~ s/^\s+//;
	my @words = parse_line('\s+', 0, $line);
	pop @words if (@words and !defined $words[-1]);
	return() unless (@words || !length($line));
	push(@allwords, @words);
    }
    return(@allwords);
}



sub quotewords {
    my($delim, $keep, @lines) = @_;
    my($line, @words, @allwords);

    foreach $line (@lines) {
	@words = parse_line($delim, $keep, $line);
	return() unless (@words || !length($line));
	push(@allwords, @words);
    }
    return(@allwords);
}



sub nested_quotewords {
    my($delim, $keep, @lines) = @_;
    my($i, @allwords);

    for ($i = 0; $i < @lines; $i++) {
	@{$allwords[$i]} = parse_line($delim, $keep, $lines[$i]);
	return() unless (@{$allwords[$i]} || !length($lines[$i]));
    }
    return(@allwords);
}



sub parse_line {
    my($delimiter, $keep, $line) = @_;
    my($word, @pieces);

    no warnings 'uninitialized';	# we will be testing undef strings

    while (length($line)) {
        # This pattern is optimised to be stack conservative on older perls.
        # Do not refactor without being careful and testing it on very long strings.
        # See Perl bug #42980 for an example of a stack busting input.
        $line =~ s/^
                    (?: 
                        # double quoted string
                        (")                             # $quote
                        ((?>[^\\"]*(?:\\.[^\\"]*)*))"   # $quoted 
		    |	# --OR--
                        # singe quoted string
                        (')                             # $quote
                        ((?>[^\\']*(?:\\.[^\\']*)*))'   # $quoted
                    |   # --OR--
                        # unquoted string
		        (                               # $unquoted 
                            (?:\\.|[^\\"'])*?           
                        )		
                        # followed by
		        (                               # $delim
                            \Z(?!\n)                    # EOL
                        |   # --OR--
                            (?-x:$delimiter)            # delimiter
                        |   # --OR--                    
                            (?!^)(?=["'])               # a quote
                        )  
		    )//xs or return;		# extended layout                  
        my ($quote, $quoted, $unquoted, $delim) = (($1 ? ($1,$2) : ($3,$4)), $5, $6);


	return() unless( defined($quote) || length($unquoted) || length($delim));

        if ($keep) {
	    $quoted = "$quote$quoted$quote";
	}
        else {
	    $unquoted =~ s/\\(.)/$1/sg;
	    if (defined $quote) {
		$quoted =~ s/\\(.)/$1/sg if ($quote eq '"');
		$quoted =~ s/\\([\\'])/$1/g if ( $PERL_SINGLE_QUOTE && $quote eq "'");
            }
	}
        $word .= substr($line, 0, 0);	# leave results tainted
        $word .= defined $quote ? $quoted : $unquoted;
 
        if (length($delim)) {
            push(@pieces, $word);
            push(@pieces, $delim) if ($keep eq 'delimiters');
            undef $word;
        }
        if (!length($line)) {
            push(@pieces, $word);
	}
    }
    return(@pieces);
}



sub old_shellwords {

    # Usage:
    #	use ParseWords;
    #	@words = old_shellwords($line);
    #	or
    #	@words = old_shellwords(@lines);
    #	or
    #	@words = old_shellwords();	# defaults to $_ (and clobbers it)

    no warnings 'uninitialized';	# we will be testing undef strings
    local *_ = \join('', @_) if @_;
    my (@words, $snippet);

    s/\A\s+//;
    while ($_ ne '') {
	my $field = substr($_, 0, 0);	# leave results tainted
	for (;;) {
	    if (s/\A"(([^"\\]|\\.)*)"//s) {
		($snippet = $1) =~ s#\\(.)#$1#sg;
	    }
	    elsif (/\A"/) {
		require Carp;
		Carp::carp("Unmatched double quote: $_");
		return();
	    }
	    elsif (s/\A'(([^'\\]|\\.)*)'//s) {
		($snippet = $1) =~ s#\\(.)#$1#sg;
	    }
	    elsif (/\A'/) {
		require Carp;
		Carp::carp("Unmatched single quote: $_");
		return();
	    }
	    elsif (s/\A\\(.?)//s) {
		$snippet = $1;
	    }
	    elsif (s/\A([^\s\\'"]+)//) {
		$snippet = $1;
	    }
	    else {
		s/\A\s+//;
		last;
	    }
	    $field .= $snippet;
	}
	push(@words, $field);
    }
    return @words;
}

1;

__END__

=head1 NAME

Text::ParseWords - parse text into an array of tokens or array of arrays

=head1 SYNOPSIS

  use Text::ParseWords;
  @lists = &nested_quotewords($delim, $keep, @lines);
  @words = &quotewords($delim, $keep, @lines);
  @words = &shellwords(@lines);
  @words = &parse_line($delim, $keep, $line);
  @words = &old_shellwords(@lines); # DEPRECATED!

=head1 DESCRIPTION

The &nested_quotewords() and &quotewords() functions accept a delimiter 
(which can be a regular expression)
and a list of lines and then breaks those lines up into a list of
words ignoring delimiters that appear inside quotes.  &quotewords()
returns all of the tokens in a single long list, while &nested_quotewords()
returns a list of token lists corresponding to the elements of @lines.
&parse_line() does tokenizing on a single string.  The &*quotewords()
functions simply call &parse_line(), so if you're only splitting
one line you can call &parse_line() directly and save a function
call.

The $keep argument is a boolean flag.  If true, then the tokens are
split on the specified delimiter, but all other characters (quotes,
backslashes, etc.) are kept in the tokens.  If $keep is false then the
&*quotewords() functions remove all quotes and backslashes that are
not themselves backslash-escaped or inside of single quotes (i.e.,
&quotewords() tries to interpret these characters just like the Bourne
shell).  NB: these semantics are significantly different from the
original version of this module shipped with Perl 5.000 through 5.004.
As an additional feature, $keep may be the keyword "delimiters" which
causes the functions to preserve the delimiters in each string as
tokens in the token lists, in addition to preserving quote and
backslash characters.

&shellwords() is written as a special case of &quotewords(), and it
does token parsing with whitespace as a delimiter-- similar to most
Unix shells.

=head1 EXAMPLES

The sample program:

  use Text::ParseWords;
  @words = &quotewords('\s+', 0, q{this   is "a test" of\ quotewords \"for you});
  $i = 0;
  foreach (@words) {
      print "$i: <$_>\n";
      $i++;
  }

produces:

  0: <this>
  1: <is>
  2: <a test>
  3: <of quotewords>
  4: <"for>
  5: <you>

demonstrating:

=over 4

=item 0

a simple word

=item 1

multiple spaces are skipped because of our $delim

=item 2

use of quotes to include a space in a word

=item 3

use of a backslash to include a space in a word

=item 4

use of a backslash to remove the special meaning of a double-quote

=item 5

another simple word (note the lack of effect of the
backslashed double-quote)

=back

Replacing C<&quotewords('\s+', 0, q{this   is...})>
with C<&shellwords(q{this   is...})>
is a simpler way to accomplish the same thing.

=head1 AUTHORS

Maintainer is Hal Pomeranz <pomeranz@netcom.com>, 1994-1997 (Original
author unknown).  Much of the code for &parse_line() (including the
primary regexp) from Joerk Behrends <jbehrends@multimediaproduzenten.de>.

Examples section another documentation provided by John Heidemann 
<johnh@ISI.EDU>

Bug reports, patches, and nagging provided by lots of folks-- thanks
everybody!  Special thanks to Michael Schwern <schwern@envirolink.org>
for assuring me that a &nested_quotewords() would be useful, and to 
Jeff Friedl <jfriedl@yahoo-inc.com> for telling me not to worry about
error-checking (sort of-- you had to be there).

=cut
