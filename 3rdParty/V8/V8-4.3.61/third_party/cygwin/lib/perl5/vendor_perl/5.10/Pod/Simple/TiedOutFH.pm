
use strict;
package Pod::Simple::TiedOutFH;
use Symbol ('gensym');
use Carp ();

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub handle_on { # some horrible frightening things are encapsulated in here
  my $class = shift;
  $class = ref($class) || $class;
  
  Carp::croak "Usage: ${class}->handle_on(\$somescalar)" unless @_;
  
  my $x = (defined($_[0]) and ref($_[0]))
    ? $_[0]
    : ( \( $_[0] ) )[0]
  ;
  $$x = '' unless defined $$x;
  
  #Pod::Simple::DEBUG and print "New $class handle on $x = \"$$x\"\n";
  
  my $new = gensym();
  tie *$new, $class, $x;
  return $new;
}

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

sub TIEHANDLE {  # Ties to just a scalar ref
  my($class, $scalar_ref) = @_;
  $$scalar_ref = '' unless defined $$scalar_ref;
  return bless \$scalar_ref,  ref($class) || $class;
}

sub PRINT {
  my $it = shift;
  foreach my $x (@_) { $$$it .= $x }

  #Pod::Simple::DEBUG > 10 and print " appended to $$it = \"$$$it\"\n";

  return 1;
}

sub FETCH {
  return ${$_[0]};
}

sub PRINTF {
  my $it = shift;
  my $format = shift;
  $$$it .= sprintf $format, @_;
  return 1;
}

sub FILENO { ${ $_[0] } + 100 } # just to produce SOME number

sub CLOSE { 1 }

#~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
1;
__END__

Chole

 * 1 large red onion
 * 2 tomatillos
 * 4 or 5 roma tomatoes (optionally with the pulp discarded)
 * 1 tablespoons chopped ginger root (or more, to taste)
 * 2 tablespoons canola oil (or vegetable oil)
 
 * 1 tablespoon garam masala
 * 1/2 teaspoon red chili powder, or to taste
 * Salt, to taste (probably quite a bit)
 * 2 (15-ounce) cans chick peas or garbanzo beans, drained and rinsed
 * juice of one smallish lime
 * a dash of balsamic vinegar (to taste)
 * cooked rice, preferably long-grain white rice (whether plain,
    basmati rice, jasmine rice, or even a mild pilaf)

In a blender or food processor, puree the onions, tomatoes, tomatillos,
and ginger root.  You can even do it with a Braun hand "mixer", if you
chop things finer to start with, and work at it.

In a saucepan set over moderate heat, warm the oil until hot.

Add the puree and the balsamic vinegar, and cook, stirring occasionally,
for 20 to 40 minutes. (Cooking it longer will make it sweeter.)

Add the Garam Masala, chili powder, and cook, stirring occasionally, for
5 minutes.

Add the salt and chick peas and cook, stirring, until heated through.

Stir in the lime juice, and optionally one or two teaspoons of tahini.
You can let it simmer longer, depending on how much softer you want the
garbanzos to get.

Serve over rice, like a curry.

Yields 5 to 7 servings.


