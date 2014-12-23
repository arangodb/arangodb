
package Text::Tabs;

require Exporter;

@ISA = (Exporter);
@EXPORT = qw(expand unexpand $tabstop);

use vars qw($VERSION $tabstop $debug);
$VERSION = 2007.1117;

use strict;

BEGIN	{
	$tabstop = 8;
	$debug = 0;
}

sub expand {
	my @l;
	my $pad;
	for ( @_ ) {
		my $s = '';
		for (split(/^/m, $_, -1)) {
			my $offs = 0;
			s{\t}{
				$pad = $tabstop - (pos() + $offs) % $tabstop;
				$offs += $pad - 1;
				" " x $pad;
			}eg;
			$s .= $_;
		}
		push(@l, $s);
	}
	return @l if wantarray;
	return $l[0];
}

sub unexpand
{
	my (@l) = @_;
	my @e;
	my $x;
	my $line;
	my @lines;
	my $lastbit;
	my $ts_as_space = " "x$tabstop;
	for $x (@l) {
		@lines = split("\n", $x, -1);
		for $line (@lines) {
			$line = expand($line);
			@e = split(/(.{$tabstop})/,$line,-1);
			$lastbit = pop(@e);
			$lastbit = '' 
				unless defined $lastbit;
			$lastbit = "\t"
				if $lastbit eq $ts_as_space;
			for $_ (@e) {
				if ($debug) {
					my $x = $_;
					$x =~ s/\t/^I\t/gs;
					print "sub on '$x'\n";
				}
				s/  +$/\t/;
			}
			$line = join('',@e, $lastbit);
		}
		$x = join("\n", @lines);
	}
	return @l if wantarray;
	return $l[0];
}

1;
__END__

sub expand
{
	my (@l) = @_;
	for $_ (@l) {
		1 while s/(^|\n)([^\t\n]*)(\t+)/
			$1. $2 . (" " x 
				($tabstop * length($3)
				- (length($2) % $tabstop)))
			/sex;
	}
	return @l if wantarray;
	return $l[0];
}


=head1 NAME

Text::Tabs -- expand and unexpand tabs per the unix expand(1) and unexpand(1)

=head1 SYNOPSIS

  use Text::Tabs;

  $tabstop = 4;  # default = 8
  @lines_without_tabs = expand(@lines_with_tabs);
  @lines_with_tabs = unexpand(@lines_without_tabs);

=head1 DESCRIPTION

Text::Tabs does about what the unix utilities expand(1) and unexpand(1) 
do.  Given a line with tabs in it, expand will replace the tabs with
the appropriate number of spaces.  Given a line with or without tabs in
it, unexpand will add tabs when it can save bytes by doing so (just
like C<unexpand -a>).  Invisible compression with plain ASCII! 

=head1 EXAMPLE

  #!perl
  # unexpand -a
  use Text::Tabs;

  while (<>) {
    print unexpand $_;
  }

Instead of the C<expand> comand, use:

  perl -MText::Tabs -n -e 'print expand $_'

Instead of the C<unexpand -a> command, use:

  perl -MText::Tabs -n -e 'print unexpand $_'

=head1 LICENSE

Copyright (C) 1996-2002,2005,2006 David Muir Sharnoff.  
Copyright (C) 2005 Aristotle Pagaltzis 
This module may be modified, used, copied, and redistributed at your own risk.
Publicly redistributed modified versions must use a different name.

