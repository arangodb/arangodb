
require 5;
package Pod::Perldoc::GetOptsOO;
use strict;

# Rather like Getopt::Std's getopts
#  Call Pod::Perldoc::GetOptsOO::getopts($object, \@ARGV, $truth)
#  Given -n, if there's a opt_n_with, it'll call $object->opt_n_with( ARGUMENT )
#    (e.g., "-n foo" => $object->opt_n_with('foo').  Ditto "-nfoo")
#  Otherwise (given -n) if there's an opt_n, we'll call it $object->opt_n($truth)
#    (Truth defaults to 1)
#  Otherwise we try calling $object->handle_unknown_option('n')
#    (and we increment the error count by the return value of it)
#  If there's no handle_unknown_option, then we just warn, and then increment
#    the error counter
# 
#  The return value of Pod::Perldoc::GetOptsOO::getopts is true if no errors,
#   otherwise it's false.
#
## sburke@cpan.org 2002-10-31

BEGIN { # Make a DEBUG constant ASAP
  *DEBUG = defined( &Pod::Perldoc::DEBUG )
   ? \&Pod::Perldoc::DEBUG
   : sub(){10};
}


sub getopts {
  my($target, $args, $truth) = @_;
  
  $args ||= \@ARGV;

  $target->aside(
    "Starting switch processing.  Scanning arguments [@$args]\n"
  ) if $target->can('aside');

  return unless @$args;

  $truth = 1 unless @_ > 2;

  DEBUG > 3 and print "   Truth is $truth\n";


  my $error_count = 0;

  while( @$args  and  ($_ = $args->[0]) =~ m/^-(.)(.*)/s ) {
    my($first,$rest) = ($1,$2);
    if ($_ eq '--') {	# early exit if "--"
      shift @$args;
      last;
    }
    my $method = "opt_${first}_with";
    if( $target->can($method) ) {  # it's argumental
      if($rest eq '') {   # like -f bar
        shift @$args;
        warn "Option $first needs a following argument!\n" unless @$args;
        $rest = shift @$args;
      } else {            # like -fbar  (== -f bar)
        shift @$args;
      }

      DEBUG > 3 and print " $method => $rest\n";
      $target->$method( $rest );

    # Otherwise, it's not argumental...
    } else {

      if( $target->can( $method = "opt_$first" ) ) {
        DEBUG > 3 and print " $method is true ($truth)\n";
        $target->$method( $truth );

      # Otherwise it's an unknown option...

      } elsif( $target->can('handle_unknown_option') ) {
        DEBUG > 3
         and print " calling handle_unknown_option('$first')\n";
         
        $error_count += (
          $target->handle_unknown_option( $first ) || 0
        );

      } else {
        ++$error_count;
        warn "Unknown option: $first\n";
      }

      if($rest eq '') {   # like -f
        shift @$args
      } else {            # like -fbar  (== -f -bar )
        DEBUG > 2 and print "   Setting args->[0] to \"-$rest\"\n";
        $args->[0] = "-$rest";
      }
    }
  }
  

  $target->aside(
    "Ending switch processing.  Args are [@$args] with $error_count errors.\n"
  ) if $target->can('aside');

  $error_count == 0;
}

1;

