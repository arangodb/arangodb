package DBM_Filter ;

use strict;
use warnings;
our $VERSION = '0.02';

package Tie::Hash ;

use strict;
use warnings;

use Carp;


our %LayerStack = ();
our %origDESTROY = ();

our %Filters = map { $_, undef } qw(
            Fetch_Key
            Fetch_Value
            Store_Key
            Store_Value
	);

our %Options = map { $_, 1 } qw(
            fetch
            store
	);

#sub Filter_Enable
#{
#}
#
#sub Filter_Disable
#{
#}

sub Filtered
{
    my $this = shift;
    return defined $LayerStack{$this} ;
}

sub Filter_Pop
{
    my $this = shift;
    my $stack = $LayerStack{$this} || return undef ;
    my $filter = pop @{ $stack };

    # remove the filter hooks if this is the last filter to pop
    if ( @{ $stack } == 0 ) {
        $this->filter_store_key  ( undef );
        $this->filter_store_value( undef );
        $this->filter_fetch_key  ( undef );
        $this->filter_fetch_value( undef );
        delete $LayerStack{$this};
    }

    return $filter;
}

sub Filter_Key_Push
{
    &_do_Filter_Push;
}

sub Filter_Value_Push
{
    &_do_Filter_Push;
}


sub Filter_Push
{
    &_do_Filter_Push;
}

sub _do_Filter_Push
{
    my $this = shift;
    my %callbacks = ();
    my $caller = (caller(1))[3];
    $caller =~ s/^.*:://;
 
    croak "$caller: no parameters present" unless @_ ;

    if ( ! $Options{lc $_[0]} ) {
        my $class = shift;
        my @params = @_;

        # if $class already contains "::", don't prefix "DBM_Filter::"
        $class = "DBM_Filter::$class" unless $class =~ /::/;
    
        no strict 'refs';
        # does the "DBM_Filter::$class" exist?
	if ( ! defined %{ "${class}::"} ) {
	    # Nope, so try to load it.
            eval " require $class ; " ;
            croak "$caller: Cannot Load DBM Filter '$class': $@" if $@;
        }
    
        my $fetch  = *{ "${class}::Fetch"  }{CODE};
        my $store  = *{ "${class}::Store"  }{CODE};
        my $filter = *{ "${class}::Filter" }{CODE};
        use strict 'refs';

        my $count = defined($filter) + defined($store) + defined($fetch) ;

        if ( $count == 0 )
          { croak "$caller: No methods (Filter, Fetch or Store) found in class '$class'" }
        elsif ( $count == 1 && ! defined $filter) {
           my $need = defined($fetch) ? 'Store' : 'Fetch';
           croak "$caller: Missing method '$need' in class '$class'" ;
        }
        elsif ( $count >= 2 && defined $filter)
          { croak "$caller: Can't mix Filter with Store and Fetch in class '$class'" }

        if (defined $filter) {
            my $callbacks = &{ $filter }(@params);
            croak "$caller: '${class}::Filter' did not return a hash reference" 
                unless ref $callbacks && ref $callbacks eq 'HASH';
            %callbacks = %{ $callbacks } ;
        }
        else {
            $callbacks{Fetch} = $fetch;
            $callbacks{Store} = $store;
        }
    }
    else {
        croak "$caller: not even params" unless @_ % 2 == 0;
        %callbacks = @_;
    }
    
    my %filters = %Filters ;
    my @got = ();
    while (my ($k, $v) = each %callbacks )
    {
        my $key = $k;
        $k = lc $k;
        if ($k eq 'fetch') {
            push @got, 'Fetch';
            if ($caller eq 'Filter_Push')
              { $filters{Fetch_Key} = $filters{Fetch_Value} = $v }
            elsif ($caller eq 'Filter_Key_Push')
              { $filters{Fetch_Key} = $v }
            elsif ($caller eq 'Filter_Value_Push')
              { $filters{Fetch_Value} = $v }
        }
        elsif ($k eq 'store') {
            push @got, 'Store';
            if ($caller eq 'Filter_Push')
              { $filters{Store_Key} = $filters{Store_Value} = $v }
            elsif ($caller eq 'Filter_Key_Push')
              { $filters{Store_Key} = $v }
            elsif ($caller eq 'Filter_Value_Push')
              { $filters{Store_Value} = $v }
        }
        else
          { croak "$caller: Unknown key '$key'" }

        croak "$caller: value associated with key '$key' is not a code reference"
            unless ref $v && ref $v eq 'CODE';
    }

    if ( @got != 2 ) {
        push @got, 'neither' if @got == 0 ;
        croak "$caller: expected both Store & Fetch - got @got";
    }

    # remember the class
    push @{ $LayerStack{$this} }, \%filters ;

    my $str_this = "$this" ; # Avoid a closure with $this in the subs below

    $this->filter_store_key  ( sub { store_hook($str_this, 'Store_Key')   });
    $this->filter_store_value( sub { store_hook($str_this, 'Store_Value') });
    $this->filter_fetch_key  ( sub { fetch_hook($str_this, 'Fetch_Key')   });
    $this->filter_fetch_value( sub { fetch_hook($str_this, 'Fetch_Value') });

    # Hijack the callers DESTROY method
    $this =~ /^(.*)=/;
    my $type = $1 ;
    no strict 'refs';
    if ( *{ "${type}::DESTROY" }{CODE} ne \&MyDESTROY )
    {
        $origDESTROY{$type} = *{ "${type}::DESTROY" }{CODE};
        no warnings 'redefine';
        *{ "${type}::DESTROY" } = \&MyDESTROY ;
    }
}

sub store_hook
{
    my $this = shift ;
    my $type = shift ;
    foreach my $layer (@{ $LayerStack{$this} })
    {
        &{ $layer->{$type} }() if defined $layer->{$type} ;
    }
}

sub fetch_hook
{
    my $this = shift ;
    my $type = shift ;
    foreach my $layer (reverse @{ $LayerStack{$this} })
    {
        &{ $layer->{$type} }() if defined $layer->{$type} ;
    }
}

sub MyDESTROY
{
    my $this = shift ;
    delete $LayerStack{$this} ;

    # call real DESTROY
    $this =~ /^(.*)=/;
    &{ $origDESTROY{$1} }($this);
}

1;

__END__

=head1 NAME

DBM_Filter -- Filter DBM keys/values 

=head1 SYNOPSIS

    use DBM_Filter ;
    use SDBM_File; # or DB_File, or GDBM_File, or NDBM_File, or ODBM_File

    $db = tie %hash, ...

    $db->Filter_Push(Fetch => sub {...},
                     Store => sub {...});

    $db->Filter_Push('my_filter1');
    $db->Filter_Push('my_filter2', params...);

    $db->Filter_Key_Push(...) ;
    $db->Filter_Value_Push(...) ;

    $db->Filter_Pop();
    $db->Filtered();

    package DBM_Filter::my_filter1;
    
    sub Store { ... }
    sub Fetch { ... }

    1;

    package DBM_Filter::my_filter2;

    sub Filter
    {
        my @opts = @_;
        ...
        return (
            sub Store { ... },
            sub Fetch { ... } );
    }

    1;

=head1 DESCRIPTION

This module provides an interface that allows filters to be applied
to tied Hashes associated with DBM files. It builds on the DBM Filter
hooks that are present in all the *DB*_File modules included with the
standard Perl source distribution from version 5.6.1 onwards. In addition
to the *DB*_File modules distributed with Perl, the BerkeleyDB module,
available on CPAN, supports the DBM Filter hooks. See L<perldbmfilter>
for more details on the DBM Filter hooks.

=head1 What is a DBM Filter?

A DBM Filter allows the keys and/or values in a tied hash to be modified
by some user-defined code just before it is written to the DBM file and
just after it is read back from the DBM file. For example, this snippet
of code

    $some_hash{"abc"} = 42;

could potentially trigger two filters, one for the writing of the key
"abc" and another for writing the value 42.  Similarly, this snippet

    my ($key, $value) = each %some_hash

will trigger two filters, one for the reading of the key and one for
the reading of the value.

Like the existing DBM Filter functionality, this module arranges for the
C<$_> variable to be populated with the key or value that a filter will
check. This usually means that most DBM filters tend to be very short.

=head2 So what's new?

The main enhancements over the standard DBM Filter hooks are:

=over 4

=item *

A cleaner interface.

=item *

The ability to easily apply multiple filters to a single DBM file.

=item *

The ability to create "canned" filters. These allow commonly used filters
to be packaged into a stand-alone module.

=back

=head1 METHODS

This module will arrange for the following methods to be available via
the object returned from the C<tie> call.

=head2 $db->Filter_Push()

=head2 $db->Filter_Key_Push()

=head2 $db->Filter_Value_Push()

Add a filter to filter stack for the database, C<$db>. The three formats
vary only in whether they apply to the DBM key, the DBM value or both.

=over 5

=item Filter_Push

The filter is applied to I<both> keys and values.

=item Filter_Key_Push

The filter is applied to the key I<only>.

=item Filter_Value_Push

The filter is applied to the value I<only>.

=back


=head2 $db->Filter_Pop()

Removes the last filter that was applied to the DBM file associated with
C<$db>, if present.

=head2 $db->Filtered()

Returns TRUE if there are any filters applied to the DBM associated
with C<$db>.  Otherwise returns FALSE.



=head1 Writing a Filter

Filters can be created in two main ways

=head2 Immediate Filters

An immediate filter allows you to specify the filter code to be used
at the point where the filter is applied to a dbm. In this mode the
Filter_*_Push methods expects to receive exactly two parameters.

    my $db = tie %hash, 'SDBM_File', ...
    $db->Filter_Push( Store => sub { },
                      Fetch => sub { });

The code reference associated with C<Store> will be called before any
key/value is written to the database and the code reference associated
with C<Fetch> will be called after any key/value is read from the
database.

For example, here is a sample filter that adds a trailing NULL character
to all strings before they are written to the DBM file, and removes the
trailing NULL when they are read from the DBM file

    my $db = tie %hash, 'SDBM_File', ...
    $db->Filter_Push( Store => sub { $_ .= "\x00" ; },
                      Fetch => sub { s/\x00$// ;    });


Points to note:

=over 5

=item 1.

Both the Store and Fetch filters manipulate C<$_>.

=back

=head2 Canned Filters

Immediate filters are useful for one-off situations. For more generic
problems it can be useful to package the filter up in its own module.

The usage is for a canned filter is:

    $db->Filter_Push("name", params)

where

=over 5

=item "name"

is the name of the module to load. If the string specified does not
contain the package separator characters "::", it is assumed to refer to
the full module name "DBM_Filter::name". This means that the full names
for canned filters, "null" and "utf8", included with this module are:

    DBM_Filter::null
    DBM_Filter::utf8

=item params

any optional parameters that need to be sent to the filter. See the
encode filter for an example of a module that uses parameters.

=back

The module that implements the canned filter can take one of two
forms. Here is a template for the first

    package DBM_Filter::null ;

    use strict;
    use warnings;

    sub Store 
    {
        # store code here    
    }

    sub Fetch
    {
        # fetch code here
    }

    1;


Notes:

=over 5

=item 1.

The package name uses the C<DBM_Filter::> prefix.

=item 2.

The module I<must> have both a Store and a Fetch method. If only one is
present, or neither are present, a fatal error will be thrown.

=back

The second form allows the filter to hold state information using a
closure, thus:

    package DBM_Filter::encoding ;

    use strict;
    use warnings;

    sub Filter
    {
        my @params = @_ ;

        ...
        return {
            Store   => sub { $_ = $encoding->encode($_) },
            Fetch   => sub { $_ = $encoding->decode($_) }
            } ;
    }

    1;


In this instance the "Store" and "Fetch" methods are encapsulated inside a
"Filter" method.


=head1 Filters Included

A number of canned filers are provided with this module. They cover a
number of the main areas that filters are needed when interfacing with
DBM files. They also act as templates for your own filters.

The filter included are:

=over 5

=item * utf8

This module will ensure that all data written to the DBM will be encoded
in UTF-8.

This module needs the Encode module.

=item * encode

Allows you to choose the character encoding will be store in the DBM file.

=item * compress

This filter will compress all data before it is written to the database
and uncompressed it on reading.

This module needs Compress::Zlib. 

=item * int32

This module is used when interoperating with a C/C++ application that
uses a C int as either the key and/or value in the DBM file.

=item * null

This module ensures that all data written to the DBM file is null
terminated. This is useful when you have a perl script that needs
to interoperate with a DBM file that a C program also uses. A fairly
common issue is for the C application to include the terminating null
in a string when it writes to the DBM file. This filter will ensure that
all data written to the DBM file can be read by the C application.

=back

=head1 NOTES

=head2 Maintain Round Trip Integrity

When writing a DBM filter it is I<very> important to ensure that it is
possible to retrieve all data that you have written when the DBM filter
is in place. In practice, this means that whatever transformation is
applied to the data in the Store method, the I<exact> inverse operation
should be applied in the Fetch method.

If you don't provide an exact inverse transformation, you will find that
code like this will not behave as you expect.

     while (my ($k, $v) = each %hash)
     {
         ...
     }

Depending on the transformation, you will find that one or more of the
following will happen

=over 5

=item 1

The loop will never terminate.

=item 2

Too few records will be retrieved.

=item 3

Too many will be retrieved.

=item 4

The loop will do the right thing for a while, but it will unexpectedly fail. 

=back

=head2 Don't mix filtered & non-filtered data in the same database file. 

This is just a restatement of the previous section. Unless you are
completely certain you know what you are doing, avoid mixing filtered &
non-filtered data.

=head1 EXAMPLE

Say you need to interoperate with a legacy C application that stores
keys as C ints and the values and null terminated UTF-8 strings. Here
is how you would set that up

    my $db = tie %hash, 'SDBM_File', ...

    $db->Filter_Key_Push('int32') ;

    $db->Filter_Value_Push('utf8');
    $db->Filter_Value_Push('null');

=head1 SEE ALSO

<DB_File>,  L<GDBM_File>, L<NDBM_File>, L<ODBM_File>, L<SDBM_File>, L<perldbmfilter>

=head1 AUTHOR

Paul Marquess <pmqs@cpan.org>

