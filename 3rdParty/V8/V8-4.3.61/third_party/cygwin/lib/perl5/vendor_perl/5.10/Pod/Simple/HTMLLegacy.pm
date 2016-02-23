
require 5;
package Pod::Simple::HTMLLegacy;
use strict;

use vars qw($VERSION);
use Getopt::Long;

$VERSION = "5.01";

#--------------------------------------------------------------------------
# 
# This class is meant to thinly emulate bad old Pod::Html
#
# TODO: some basic docs

sub pod2html {
  my @args = (@_);
  
  my( $verbose, $infile, $outfile, $title );
  my $index = 1;
 
  {
    my($help);

    my($netscape); # dummy
    local @ARGV = @args;
    GetOptions(
      "help"       => \$help,
      "verbose!"   => \$verbose,
      "infile=s"   => \$infile,
      "outfile=s"  => \$outfile,
      "title=s"    => \$title,
      "index!"     => \$index,

      "netscape!"   => \$netscape,
    ) or return bad_opts(@args);
    bad_opts(@args) if @ARGV; # it should be all switches!
    return help_message() if $help;
  }

  for($infile, $outfile) { $_ = undef unless defined and length }
  
  if($verbose) {
    warn sprintf "%s version %s\n", __PACKAGE__, $VERSION;
    warn "OK, processed args [@args] ...\n";
    warn sprintf
      " Verbose: %s\n Index: %s\n Infile: %s\n Outfile: %s\n Title: %s\n",
      map defined($_) ? $_ : "(nil)",
       $verbose,     $index,     $infile,     $outfile,     $title,
    ;
    *Pod::Simple::HTML::DEBUG = sub(){1};
  }
  require Pod::Simple::HTML;
  Pod::Simple::HTML->VERSION(3);
  
  die "No such input file as $infile\n"
   if defined $infile and ! -e $infile;

  
  my $pod = Pod::Simple::HTML->new;
  $pod->force_title($title) if defined $title;
  $pod->index($index);
  return $pod->parse_from_file($infile, $outfile);
}

#--------------------------------------------------------------------------

sub bad_opts     { die _help_message();         }
sub help_message { print STDOUT _help_message() }

#--------------------------------------------------------------------------

sub _help_message {

  join '',

"[", __PACKAGE__, " version ", $VERSION, qq~]
Usage:  pod2html --help --infile=<name> --outfile=<name>
   --verbose --index --noindex

Options:
  --help         - prints this message.
  --[no]index    - generate an index at the top of the resulting html
                   (default behavior).
  --infile       - filename for the pod to convert (input taken from stdin
                   by default).
  --outfile      - filename for the resulting html file (output sent to
                   stdout by default).
  --title        - title that will appear in resulting html file.
  --[no]verbose  - self-explanatory (off by default).

Note that pod2html is DEPRECATED, and this version implements only
 some of the options known to older versions.
For more information, see 'perldoc pod2html'.
~;

}

1;
__END__

OVER the underpass! UNDER the overpass! Around the FUTURE and BEYOND REPAIR!!

