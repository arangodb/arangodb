#!/usr/bin/env perl
use strict;
use warnings;

# Generate Pascal stemwords source.

my @sources = @ARGV;

while (defined(my $line = <STDIN>)) {
	if ($line =~ /\{\s*BEGIN TEMPLATE\s*\}/) {
		my $template = '';
		while (defined($line = <STDIN>) && $line !~ /\{\s*END TEMPLATE\s*\}/) {
			$template .= $line;
		}
		foreach my $source(@sources) {
			my $out = $template;
			$out =~ s/%STEMMER%/$source/g;
			print $out;
		}
		next;
	}
	print $line;
}
