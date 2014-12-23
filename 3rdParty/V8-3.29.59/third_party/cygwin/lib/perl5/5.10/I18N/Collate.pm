package I18N::Collate;

use strict;
our $VERSION = '1.00';

=head1 NAME

I18N::Collate - compare 8-bit scalar data according to the current locale

=head1 SYNOPSIS

    use I18N::Collate;
    setlocale(LC_COLLATE, 'locale-of-your-choice'); 
    $s1 = new I18N::Collate "scalar_data_1";
    $s2 = new I18N::Collate "scalar_data_2";

=head1 DESCRIPTION

  ***

  WARNING: starting from the Perl version 5.003_06
  the I18N::Collate interface for comparing 8-bit scalar data
  according to the current locale

	HAS BEEN DEPRECATED

  That is, please do not use it anymore for any new applications
  and please migrate the old applications away from it because its
  functionality was integrated into the Perl core language in the
  release 5.003_06.

  See the perllocale manual page for further information.

  ***

This module provides you with objects that will collate 
according to your national character set, provided that the 
POSIX setlocale() function is supported on your system.

You can compare $s1 and $s2 above with

    $s1 le $s2

to extract the data itself, you'll need a dereference: $$s1

This module uses POSIX::setlocale(). The basic collation conversion is
done by strxfrm() which terminates at NUL characters being a decent C
routine.  collate_xfrm() handles embedded NUL characters gracefully.

The available locales depend on your operating system; try whether
C<locale -a> shows them or man pages for "locale" or "nlsinfo" or the
direct approach C<ls /usr/lib/nls/loc> or C<ls /usr/lib/nls> or
C<ls /usr/lib/locale>.  Not all the locales that your vendor supports
are necessarily installed: please consult your operating system's
documentation and possibly your local system administration.  The
locale names are probably something like C<xx_XX.(ISO)?8859-N> or
C<xx_XX.(ISO)?8859N>, for example C<fr_CH.ISO8859-1> is the Swiss (CH)
variant of French (fr), ISO Latin (8859) 1 (-1) which is the Western
European character set.

=cut

# I18N::Collate.pm
#
# Author:	Jarkko Hietaniemi <F<jhi@iki.fi>>
#		Helsinki University of Technology, Finland
#
# Acks:		Guy Decoux <F<decoux@moulon.inra.fr>> understood
#		overloading magic much deeper than I and told
#		how to cut the size of this code by more than half.
#		(my first version did overload all of lt gt eq le ge cmp)
#
# Purpose:      compare 8-bit scalar data according to the current locale
#
# Requirements:	Perl5 POSIX::setlocale() and POSIX::strxfrm()
#
# Exports:	setlocale 1)
#		collate_xfrm 2)
#
# Overloads:	cmp # 3)
#
# Usage:	use I18N::Collate;
#	        setlocale(LC_COLLATE, 'locale-of-your-choice'); # 4)
#		$s1 = new I18N::Collate "scalar_data_1";
#		$s2 = new I18N::Collate "scalar_data_2";
#		
#		now you can compare $s1 and $s2: $s1 le $s2
#		to extract the data itself, you need to deref: $$s1
#		
# Notes:	
#		1) this uses POSIX::setlocale
#		2) the basic collation conversion is done by strxfrm() which
#		   terminates at NUL characters being a decent C routine.
#		   collate_xfrm handles embedded NUL characters gracefully.
#		3) due to cmp and overload magic, lt le eq ge gt work also
#		4) the available locales depend on your operating system;
#		   try whether "locale -a" shows them or man pages for
#		   "locale" or "nlsinfo" work or the more direct
#		   approach "ls /usr/lib/nls/loc" or "ls /usr/lib/nls".
#		   Not all the locales that your vendor supports
#		   are necessarily installed: please consult your
#		   operating system's documentation.
#		   The locale names are probably something like
#		   'xx_XX.(ISO)?8859-N' or 'xx_XX.(ISO)?8859N',
#		   for example 'fr_CH.ISO8859-1' is the Swiss (CH)
#		   variant of French (fr), ISO Latin (8859) 1 (-1)
#		   which is the Western European character set.
#
# Updated:	19961005
#
# ---

use POSIX qw(strxfrm LC_COLLATE);
use warnings::register;

require Exporter;

our @ISA = qw(Exporter);
our @EXPORT = qw(collate_xfrm setlocale LC_COLLATE);
our @EXPORT_OK = qw();

use overload qw(
fallback	1
cmp		collate_cmp
);

our($LOCALE, $C);

our $please_use_I18N_Collate_even_if_deprecated = 0;
sub new {
  my $new = $_[1];

  if (warnings::enabled() && $] >= 5.003_06) {
    unless ($please_use_I18N_Collate_even_if_deprecated) {
      warnings::warn <<___EOD___;
***

  WARNING: starting from the Perl version 5.003_06
  the I18N::Collate interface for comparing 8-bit scalar data
  according to the current locale

	HAS BEEN DEPRECATED

  That is, please do not use it anymore for any new applications
  and please migrate the old applications away from it because its
  functionality was integrated into the Perl core language in the
  release 5.003_06.

  See the perllocale manual page for further information.

***
___EOD___
      $please_use_I18N_Collate_even_if_deprecated++;
    }
  }

  bless \$new;
}

sub setlocale {
 my ($category, $locale) = @_[0,1];

 POSIX::setlocale($category, $locale) if (defined $category);
 # the current $LOCALE 
 $LOCALE = $locale || $ENV{'LC_COLLATE'} || $ENV{'LC_ALL'} || '';
}

sub C {
  my $s = ${$_[0]};

  $C->{$LOCALE}->{$s} = collate_xfrm($s)
    unless (defined $C->{$LOCALE}->{$s}); # cache when met

  $C->{$LOCALE}->{$s};
}

sub collate_xfrm {
  my $s = $_[0];
  my $x = '';
  
  for (split(/(\000+)/, $s)) {
    $x .= (/^\000/) ? $_ : strxfrm("$_\000");
  }

  $x;
}

sub collate_cmp {
  &C($_[0]) cmp &C($_[1]);
}

# init $LOCALE

&I18N::Collate::setlocale();

1; # keep require happy
