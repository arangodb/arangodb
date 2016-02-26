
# Call.pm
#
# Copyright (c) 1995-2001 Paul Marquess. All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.
 
package Filter::Util::Call ;

require 5.002 ;
require DynaLoader;
require Exporter;
use Carp ;
use strict;
use warnings;
use vars qw($VERSION @ISA @EXPORT) ;

@ISA = qw(Exporter DynaLoader);
@EXPORT = qw( filter_add filter_del filter_read filter_read_exact) ;
$VERSION = "1.07" ;

sub filter_read_exact($)
{
    my ($size)   = @_ ;
    my ($left)   = $size ;
    my ($status) ;

    croak ("filter_read_exact: size parameter must be > 0")
	unless $size > 0 ;

    # try to read a block which is exactly $size bytes long
    while ($left and ($status = filter_read($left)) > 0) {
        $left = $size - length $_ ;
    }

    # EOF with pending data is a special case
    return 1 if $status == 0 and length $_ ;

    return $status ;
}

sub filter_add($)
{
    my($obj) = @_ ;

    # Did we get a code reference?
    my $coderef = (ref $obj eq 'CODE') ;

    # If the parameter isn't already a reference, make it one.
    $obj = \$obj unless ref $obj ;

    $obj = bless ($obj, (caller)[0]) unless $coderef ;

    # finish off the installation of the filter in C.
    Filter::Util::Call::real_import($obj, (caller)[0], $coderef) ;
}

bootstrap Filter::Util::Call ;

1;
__END__

=head1 NAME

Filter::Util::Call - Perl Source Filter Utility Module

=head1 SYNOPSIS

    use Filter::Util::Call ;

=head1 DESCRIPTION

This module provides you with the framework to write I<Source Filters>
in Perl. 

An alternate interface to Filter::Util::Call is now available. See
L<Filter::Simple> for more details.

A I<Perl Source Filter> is implemented as a Perl module. The structure
of the module can take one of two broadly similar formats. To
distinguish between them, the first will be referred to as I<method
filter> and the second as I<closure filter>.

Here is a skeleton for the I<method filter>:

    package MyFilter ;

    use Filter::Util::Call ;

    sub import
    {
        my($type, @arguments) = @_ ;
        filter_add([]) ;
    }

    sub filter
    {
        my($self) = @_ ;
        my($status) ;

        $status = filter_read() ;
        $status ;
    }

    1 ;

and this is the equivalent skeleton for the I<closure filter>:

    package MyFilter ;

    use Filter::Util::Call ;

    sub import
    {
        my($type, @arguments) = @_ ;

        filter_add(
            sub 
            {
                my($status) ;
                $status = filter_read() ;
                $status ;
            } )
    }

    1 ;

To make use of either of the two filter modules above, place the line
below in a Perl source file.

    use MyFilter; 

In fact, the skeleton modules shown above are fully functional I<Source
Filters>, albeit fairly useless ones. All they does is filter the
source stream without modifying it at all.

As you can see both modules have a broadly similar structure. They both
make use of the C<Filter::Util::Call> module and both have an C<import>
method. The difference between them is that the I<method filter>
requires a I<filter> method, whereas the I<closure filter> gets the
equivalent of a I<filter> method with the anonymous sub passed to
I<filter_add>.

To make proper use of the I<closure filter> shown above you need to
have a good understanding of the concept of a I<closure>. See
L<perlref> for more details on the mechanics of I<closures>.

=head2 B<use Filter::Util::Call>

The following functions are exported by C<Filter::Util::Call>:

    filter_add()
    filter_read()
    filter_read_exact()
    filter_del()

=head2 B<import()>

The C<import> method is used to create an instance of the filter. It is
called indirectly by Perl when it encounters the C<use MyFilter> line
in a source file (See L<perlfunc/import> for more details on
C<import>).

It will always have at least one parameter automatically passed by Perl
- this corresponds to the name of the package. In the example above it
will be C<"MyFilter">.

Apart from the first parameter, import can accept an optional list of
parameters. These can be used to pass parameters to the filter. For
example:

    use MyFilter qw(a b c) ;

will result in the C<@_> array having the following values:

    @_ [0] => "MyFilter"
    @_ [1] => "a"
    @_ [2] => "b"
    @_ [3] => "c"

Before terminating, the C<import> function must explicitly install the
filter by calling C<filter_add>.

B<filter_add()>

The function, C<filter_add>, actually installs the filter. It takes one
parameter which should be a reference. The kind of reference used will
dictate which of the two filter types will be used.

If a CODE reference is used then a I<closure filter> will be assumed.

If a CODE reference is not used, a I<method filter> will be assumed.
In a I<method filter>, the reference can be used to store context
information. The reference will be I<blessed> into the package by
C<filter_add>.

See the filters at the end of this documents for examples of using
context information using both I<method filters> and I<closure
filters>.

=head2 B<filter() and anonymous sub>

Both the C<filter> method used with a I<method filter> and the
anonymous sub used with a I<closure filter> is where the main
processing for the filter is done.

The big difference between the two types of filter is that the I<method
filter> uses the object passed to the method to store any context data,
whereas the I<closure filter> uses the lexical variables that are
maintained by the closure.

Note that the single parameter passed to the I<method filter>,
C<$self>, is the same reference that was passed to C<filter_add>
blessed into the filter's package. See the example filters later on for
details of using C<$self>.

Here is a list of the common features of the anonymous sub and the
C<filter()> method.

=over 5

=item B<$_>

Although C<$_> doesn't actually appear explicitly in the sample filters
above, it is implicitly used in a number of places.

Firstly, when either C<filter> or the anonymous sub are called, a local
copy of C<$_> will automatically be created. It will always contain the
empty string at this point.

Next, both C<filter_read> and C<filter_read_exact> will append any
source data that is read to the end of C<$_>.

Finally, when C<filter> or the anonymous sub are finished processing,
they are expected to return the filtered source using C<$_>.

This implicit use of C<$_> greatly simplifies the filter.

=item B<$status>

The status value that is returned by the user's C<filter> method or
anonymous sub and the C<filter_read> and C<read_exact> functions take
the same set of values, namely:

    < 0  Error
    = 0  EOF
    > 0  OK

=item B<filter_read> and B<filter_read_exact>

These functions are used by the filter to obtain either a line or block
from the next filter in the chain or the actual source file if there
aren't any other filters.

The function C<filter_read> takes two forms:

    $status = filter_read() ;
    $status = filter_read($size) ;

The first form is used to request a I<line>, the second requests a
I<block>.

In line mode, C<filter_read> will append the next source line to the
end of the C<$_> scalar.

In block mode, C<filter_read> will append a block of data which is <=
C<$size> to the end of the C<$_> scalar. It is important to emphasise
the that C<filter_read> will not necessarily read a block which is
I<precisely> C<$size> bytes.

If you need to be able to read a block which has an exact size, you can
use the function C<filter_read_exact>. It works identically to
C<filter_read> in block mode, except it will try to read a block which
is exactly C<$size> bytes in length. The only circumstances when it
will not return a block which is C<$size> bytes long is on EOF or
error.

It is I<very> important to check the value of C<$status> after I<every>
call to C<filter_read> or C<filter_read_exact>.

=item B<filter_del>

The function, C<filter_del>, is used to disable the current filter. It
does not affect the running of the filter. All it does is tell Perl not
to call filter any more.

See L<Example 4: Using filter_del> for details.

=back

=head1 EXAMPLES

Here are a few examples which illustrate the key concepts - as such
most of them are of little practical use.

The C<examples> sub-directory has copies of all these filters
implemented both as I<method filters> and as I<closure filters>.

=head2 Example 1: A simple filter.

Below is a I<method filter> which is hard-wired to replace all
occurrences of the string C<"Joe"> to C<"Jim">. Not particularly
Useful, but it is the first example and I wanted to keep it simple.

    package Joe2Jim ;

    use Filter::Util::Call ;

    sub import
    {
        my($type) = @_ ;

        filter_add(bless []) ;
    }

    sub filter
    {
        my($self) = @_ ;
        my($status) ;

        s/Joe/Jim/g
            if ($status = filter_read()) > 0 ;
        $status ;
    }

    1 ;

Here is an example of using the filter:

    use Joe2Jim ;
    print "Where is Joe?\n" ;

And this is what the script above will print:

    Where is Jim?

=head2 Example 2: Using the context

The previous example was not particularly useful. To make it more
general purpose we will make use of the context data and allow any
arbitrary I<from> and I<to> strings to be used. This time we will use a
I<closure filter>. To reflect its enhanced role, the filter is called
C<Subst>.

    package Subst ;

    use Filter::Util::Call ;
    use Carp ;

    sub import
    {
        croak("usage: use Subst qw(from to)")
            unless @_ == 3 ;
        my ($self, $from, $to) = @_ ;
        filter_add(
            sub 
            {
                my ($status) ;
                s/$from/$to/
                    if ($status = filter_read()) > 0 ;
                $status ;
            })
    }
    1 ;

and is used like this:

    use Subst qw(Joe Jim) ;
    print "Where is Joe?\n" ;


=head2 Example 3: Using the context within the filter

Here is a filter which a variation of the C<Joe2Jim> filter. As well as
substituting all occurrences of C<"Joe"> to C<"Jim"> it keeps a count
of the number of substitutions made in the context object.

Once EOF is detected (C<$status> is zero) the filter will insert an
extra line into the source stream. When this extra line is executed it
will print a count of the number of substitutions actually made.
Note that C<$status> is set to C<1> in this case.

    package Count ;

    use Filter::Util::Call ;

    sub filter
    {
        my ($self) = @_ ;
        my ($status) ;

        if (($status = filter_read()) > 0 ) {
            s/Joe/Jim/g ;
	    ++ $$self ;
        }
	elsif ($$self >= 0) { # EOF
            $_ = "print q[Made ${$self} substitutions\n]" ;
            $status = 1 ;
	    $$self = -1 ;
        }

        $status ;
    }

    sub import
    {
        my ($self) = @_ ;
        my ($count) = 0 ;
        filter_add(\$count) ;
    }

    1 ;

Here is a script which uses it:

    use Count ;
    print "Hello Joe\n" ;
    print "Where is Joe\n" ;

Outputs:

    Hello Jim
    Where is Jim
    Made 2 substitutions

=head2 Example 4: Using filter_del

Another variation on a theme. This time we will modify the C<Subst>
filter to allow a starting and stopping pattern to be specified as well
as the I<from> and I<to> patterns. If you know the I<vi> editor, it is
the equivalent of this command:

    :/start/,/stop/s/from/to/

When used as a filter we want to invoke it like this:

    use NewSubst qw(start stop from to) ;

Here is the module.

    package NewSubst ;

    use Filter::Util::Call ;
    use Carp ;

    sub import
    {
        my ($self, $start, $stop, $from, $to) = @_ ;
        my ($found) = 0 ;
        croak("usage: use Subst qw(start stop from to)")
            unless @_ == 5 ;

        filter_add( 
            sub 
            {
                my ($status) ;

                if (($status = filter_read()) > 0) {

                    $found = 1
                        if $found == 0 and /$start/ ;

                    if ($found) {
                        s/$from/$to/ ;
                        filter_del() if /$stop/ ;
                    }

                }
                $status ;
            } )

    }

    1 ;

=head1 Filter::Simple

If you intend using the Filter::Call functionality, I would strongly
recommend that you check out Damian Conway's excellent Filter::Simple
module. Damian's module provides a much cleaner interface than
Filter::Util::Call. Although it doesn't allow the fine control that
Filter::Util::Call does, it should be adequate for the majority of
applications. It's available at

   http://www.cpan.org/modules/by-author/Damian_Conway/Filter-Simple.tar.gz
   http://www.csse.monash.edu.au/~damian/CPAN/Filter-Simple.tar.gz

=head1 AUTHOR

Paul Marquess 

=head1 DATE

26th January 1996

=cut

