package Text::Abbrev;
require 5.005;		# Probably works on earlier versions too.
require Exporter;

our $VERSION = '1.01';

=head1 NAME

abbrev - create an abbreviation table from a list

=head1 SYNOPSIS

    use Text::Abbrev;
    abbrev $hashref, LIST


=head1 DESCRIPTION

Stores all unambiguous truncations of each element of LIST
as keys in the associative array referenced by C<$hashref>.
The values are the original list elements.

=head1 EXAMPLE

    $hashref = abbrev qw(list edit send abort gripe);

    %hash = abbrev qw(list edit send abort gripe);

    abbrev $hashref, qw(list edit send abort gripe);

    abbrev(*hash, qw(list edit send abort gripe));

=cut

@ISA = qw(Exporter);
@EXPORT = qw(abbrev);

# Usage:
#	abbrev \%foo, LIST;
#	...
#	$long = $foo{$short};

sub abbrev {
    my ($word, $hashref, $glob, %table, $returnvoid);

    @_ or return;   # So we don't autovivify onto @_ and trigger warning
    if (ref($_[0])) {           # hash reference preferably
      $hashref = shift;
      $returnvoid = 1;
    } elsif (ref \$_[0] eq 'GLOB') {  # is actually a glob (deprecated)
      $hashref = \%{shift()};
      $returnvoid = 1;
    }
    %{$hashref} = ();

    WORD: foreach $word (@_) {
        for (my $len = (length $word) - 1; $len > 0; --$len) {
	    my $abbrev = substr($word,0,$len);
	    my $seen = ++$table{$abbrev};
	    if ($seen == 1) {	    # We're the first word so far to have
	    			    # this abbreviation.
	        $hashref->{$abbrev} = $word;
	    } elsif ($seen == 2) {  # We're the second word to have this
	    			    # abbreviation, so we can't use it.
	        delete $hashref->{$abbrev};
	    } else {		    # We're the third word to have this
	    			    # abbreviation, so skip to the next word.
	        next WORD;
	    }
	}
    }
    # Non-abbreviations always get entered, even if they aren't unique
    foreach $word (@_) {
        $hashref->{$word} = $word;
    }
    return if $returnvoid;
    if (wantarray) {
      %{$hashref};
    } else {
      $hashref;
    }
}

1;
