package CGI::Pretty;

# See the bottom of this file for the POD documentation.  Search for the
# string '=head'.

# You can run this file through either pod2man or pod2html to produce pretty
# documentation in manual or html file format (these utilities are part of the
# Perl 5 distribution).

use strict;
use CGI ();

$CGI::Pretty::VERSION = '1.08';
$CGI::DefaultClass = __PACKAGE__;
$CGI::Pretty::AutoloadClass = 'CGI';
@CGI::Pretty::ISA = qw( CGI );

initialize_globals();

sub _prettyPrint {
    my $input = shift;
    return if !$$input;
    return if !$CGI::Pretty::LINEBREAK || !$CGI::Pretty::INDENT;

#    print STDERR "'", $$input, "'\n";

    foreach my $i ( @CGI::Pretty::AS_IS ) {
	if ( $$input =~ m{</$i>}si ) {
	    my ( $a, $b, $c ) = $$input =~ m{(.*)(<$i[\s/>].*?</$i>)(.*)}si;
	    next if !$b;
	    $a ||= "";
	    $c ||= "";

	    _prettyPrint( \$a ) if $a;
	    _prettyPrint( \$c ) if $c;
	    
	    $b ||= "";
	    $$input = "$a$b$c";
	    return;
	}
    }
    $$input =~ s/$CGI::Pretty::LINEBREAK/$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT/g;
}

sub comment {
    my($self,@p) = CGI::self_or_CGI(@_);

    my $s = "@p";
    $s =~ s/$CGI::Pretty::LINEBREAK/$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT/g if $CGI::Pretty::LINEBREAK; 
    
    return $self->SUPER::comment( "$CGI::Pretty::LINEBREAK$CGI::Pretty::INDENT$s$CGI::Pretty::LINEBREAK" ) . $CGI::Pretty::LINEBREAK;
}

sub _make_tag_func {
    my ($self,$tagname) = @_;

    # As Lincoln as noted, the last else clause is VERY hairy, and it
    # took me a while to figure out what I was trying to do.
    # What it does is look for tags that shouldn't be indented (e.g. PRE)
    # and makes sure that when we nest tags, those tags don't get
    # indented.
    # For an example, try print td( pre( "hello\nworld" ) );
    # If we didn't care about stuff like that, the code would be
    # MUCH simpler.  BTW: I won't claim to be a regular expression
    # guru, so if anybody wants to contribute something that would
    # be quicker, easier to read, etc, I would be more than
    # willing to put it in - Brian

    my $func = qq"
	sub $tagname {";

    $func .= q'
            shift if $_[0] && 
                    (ref($_[0]) &&
                     (substr(ref($_[0]),0,3) eq "CGI" ||
                    UNIVERSAL::isa($_[0],"CGI")));
	    my($attr) = "";
	    if (ref($_[0]) && ref($_[0]) eq "HASH") {
		my(@attr) = make_attributes(shift()||undef,1);
		$attr = " @attr" if @attr;
	    }';

    if ($tagname=~/start_(\w+)/i) {
	$func .= qq! 
            return "<\L$1\E\$attr>\$CGI::Pretty::LINEBREAK";} !;
    } elsif ($tagname=~/end_(\w+)/i) {
	$func .= qq! 
            return "<\L/$1\E>\$CGI::Pretty::LINEBREAK"; } !;
    } else {
	$func .= qq#
	    return ( \$CGI::XHTML ? "<\L$tagname\E\$attr />" : "<\L$tagname\E\$attr>" ) .
                   \$CGI::Pretty::LINEBREAK unless \@_;
	    my(\$tag,\$untag) = ("<\L$tagname\E\$attr>","</\L$tagname>\E");

            my \%ASIS = map { lc("\$_") => 1 } \@CGI::Pretty::AS_IS;
            my \@args;
            if ( \$CGI::Pretty::LINEBREAK || \$CGI::Pretty::INDENT ) {
   	      if(ref(\$_[0]) eq 'ARRAY') {
                 \@args = \@{\$_[0]}
              } else {
                  foreach (\@_) {
		      \$args[0] .= \$_;
                      \$args[0] .= \$CGI::Pretty::LINEBREAK if \$args[0] !~ /\$CGI::Pretty::LINEBREAK\$/ && 0;
                      chomp \$args[0] if exists \$ASIS{ "\L$tagname\E" };
                      
  	              \$args[0] .= \$" if \$args[0] !~ /\$CGI::Pretty::LINEBREAK\$/ && 1;
		  }
                  chop \$args[0];
	      }
            }
            else {
              \@args = ref(\$_[0]) eq 'ARRAY' ? \@{\$_[0]} : "\@_";
            }

            my \@result;
            if ( exists \$ASIS{ "\L$tagname\E" } ) {
		\@result = map { "\$tag\$_\$untag\$CGI::Pretty::LINEBREAK" } 
		 \@args;
	    }
	    else {
		\@result = map { 
		    chomp; 
		    my \$tmp = \$_;
		    CGI::Pretty::_prettyPrint( \\\$tmp );
                    \$tag . \$CGI::Pretty::LINEBREAK .
                    \$CGI::Pretty::INDENT . \$tmp . \$CGI::Pretty::LINEBREAK . 
                    \$untag . \$CGI::Pretty::LINEBREAK
                } \@args;
	    }
	    local \$" = "" if \$CGI::Pretty::LINEBREAK || \$CGI::Pretty::INDENT;
	    return "\@result";
	}#;
    }    

    return $func;
}

sub start_html {
    return CGI::start_html( @_ ) . $CGI::Pretty::LINEBREAK;
}

sub end_html {
    return CGI::end_html( @_ ) . $CGI::Pretty::LINEBREAK;
}

sub new {
    my $class = shift;
    my $this = $class->SUPER::new( @_ );

    if ($CGI::MOD_PERL) {
        if ($CGI::MOD_PERL == 1) {
            my $r = Apache->request;
            $r->register_cleanup(\&CGI::Pretty::_reset_globals);
        }
        else {
            my $r = Apache2::RequestUtil->request;
            $r->pool->cleanup_register(\&CGI::Pretty::_reset_globals);
        }
    }
    $class->_reset_globals if $CGI::PERLEX;

    return bless $this, $class;
}

sub initialize_globals {
    # This is the string used for indentation of tags
    $CGI::Pretty::INDENT = "\t";
    
    # This is the string used for seperation between tags
    $CGI::Pretty::LINEBREAK = $/;

    # These tags are not prettify'd.
    @CGI::Pretty::AS_IS = qw( a pre code script textarea td );

    1;
}
sub _reset_globals { initialize_globals(); }

1;

=head1 NAME

CGI::Pretty - module to produce nicely formatted HTML code

=head1 SYNOPSIS

    use CGI::Pretty qw( :html3 );

    # Print a table with a single data element
    print table( TR( td( "foo" ) ) );

=head1 DESCRIPTION

CGI::Pretty is a module that derives from CGI.  It's sole function is to
allow users of CGI to output nicely formatted HTML code.

When using the CGI module, the following code:
    print table( TR( td( "foo" ) ) );

produces the following output:
    <TABLE><TR><TD>foo</TD></TR></TABLE>

If a user were to create a table consisting of many rows and many columns,
the resultant HTML code would be quite difficult to read since it has no
carriage returns or indentation.

CGI::Pretty fixes this problem.  What it does is add a carriage
return and indentation to the HTML code so that one can easily read
it.

    print table( TR( td( "foo" ) ) );

now produces the following output:
    <TABLE>
       <TR>
          <TD>
             foo
          </TD>
       </TR>
    </TABLE>


=head2 Tags that won't be formatted

The <A> and <PRE> tags are not formatted.  If these tags were formatted, the
user would see the extra indentation on the web browser causing the page to
look different than what would be expected.  If you wish to add more tags to
the list of tags that are not to be touched, push them onto the C<@AS_IS> array:

    push @CGI::Pretty::AS_IS,qw(CODE XMP);

=head2 Customizing the Indenting

If you wish to have your own personal style of indenting, you can change the
C<$INDENT> variable:

    $CGI::Pretty::INDENT = "\t\t";

would cause the indents to be two tabs.

Similarly, if you wish to have more space between lines, you may change the
C<$LINEBREAK> variable:

    $CGI::Pretty::LINEBREAK = "\n\n";

would create two carriage returns between lines.

If you decide you want to use the regular CGI indenting, you can easily do 
the following:

    $CGI::Pretty::INDENT = $CGI::Pretty::LINEBREAK = "";

=head1 BUGS

This section intentionally left blank.

=head1 AUTHOR

Brian Paulsen <Brian@ThePaulsens.com>, with minor modifications by
Lincoln Stein <lstein@cshl.org> for incorporation into the CGI.pm
distribution.

Copyright 1999, Brian Paulsen.  All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the same terms as Perl itself.

Bug reports and comments to Brian@ThePaulsens.com.  You can also write
to lstein@cshl.org, but this code looks pretty hairy to me and I'm not
sure I understand it!

=head1 SEE ALSO

L<CGI>

=cut
