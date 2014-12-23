package Object::Accessor;

use strict;
use Carp            qw[carp croak];
use vars            qw[$FATAL $DEBUG $AUTOLOAD $VERSION];
use Params::Check   qw[allow];
use Data::Dumper;

### some objects might have overload enabled, we'll need to
### disable string overloading for callbacks
require overload;

$VERSION    = '0.34';
$FATAL      = 0;
$DEBUG      = 0;

use constant VALUE => 0;    # array index in the hash value
use constant ALLOW => 1;    # array index in the hash value
use constant ALIAS => 2;    # array index in the hash value

=head1 NAME

Object::Accessor

=head1 SYNOPSIS

    ### using the object
    $obj = Object::Accessor->new;        # create object
    $obj = Object::Accessor->new(@list); # create object with accessors
    $obj = Object::Accessor->new(\%h);   # create object with accessors
                                         # and their allow handlers

    $bool   = $obj->mk_accessors('foo'); # create accessors
    $bool   = $obj->mk_accessors(        # create accessors with input
               {foo => ALLOW_HANDLER} ); # validation

    $bool   = $obj->mk_aliases(          # create an alias to an existing
                alias_name => 'method'); # method name
                
    $clone  = $obj->mk_clone;            # create a clone of original
                                         # object without data
    $bool   = $obj->mk_flush;            # clean out all data

    @list   = $obj->ls_accessors;        # retrieves a list of all
                                         # accessors for this object

    $bar    = $obj->foo('bar');          # set 'foo' to 'bar'
    $bar    = $obj->foo();               # retrieve 'bar' again

    $sub    = $obj->can('foo');          # retrieve coderef for
                                         # 'foo' accessor
    $bar    = $sub->('bar');             # set 'foo' via coderef
    $bar    = $sub->();                  # retrieve 'bar' by coderef

    ### using the object as base class
    package My::Class;
    use base 'Object::Accessor';

    $obj    = My::Class->new;               # create base object
    $bool   = $obj->mk_accessors('foo');    # create accessors, etc...

    ### make all attempted access to non-existant accessors fatal
    ### (defaults to false)
    $Object::Accessor::FATAL = 1;

    ### enable debugging
    $Object::Accessor::DEBUG = 1;

    ### advanced usage -- callbacks
    {   my $obj = Object::Accessor->new('foo');
        $obj->register_callback( sub { ... } );
        
        $obj->foo( 1 ); # these calls invoke the callback you registered
        $obj->foo()     # which allows you to change the get/set 
                        # behaviour and what is returned to the caller.
    }        

    ### advanced usage -- lvalue attributes
    {   my $obj = Object::Accessor::Lvalue->new('foo');
        print $obj->foo = 1;            # will print 1
    }

    ### advanced usage -- scoped attribute values
    {   my $obj = Object::Accessor->new('foo');
        
        $obj->foo( 1 );
        print $obj->foo;                # will print 1

        ### bind the scope of the value of attribute 'foo'
        ### to the scope of '$x' -- when $x goes out of 
        ### scope, 'foo's previous value will be restored
        {   $obj->foo( 2 => \my $x );
            print $obj->foo, ' ', $x;   # will print '2 2'
        }
        print $obj->foo;                # will print 1
    }


=head1 DESCRIPTION

C<Object::Accessor> provides an interface to create per object
accessors (as opposed to per C<Class> accessors, as, for example,
C<Class::Accessor> provides).

You can choose to either subclass this module, and thus using its
accessors on your own module, or to store an C<Object::Accessor>
object inside your own object, and access the accessors from there.
See the C<SYNOPSIS> for examples.

=head1 METHODS

=head2 $object = Object::Accessor->new( [ARGS] );

Creates a new (and empty) C<Object::Accessor> object. This method is
inheritable.

Any arguments given to C<new> are passed straight to C<mk_accessors>.

If you want to be able to assign to your accessors as if they
were C<lvalue>s, you should create your object in the 
C<Object::Acccessor::Lvalue> namespace instead. See the section
on C<LVALUE ACCESSORS> below.

=cut

sub new {
    my $class   = shift;
    my $obj     = bless {}, $class;
    
    $obj->mk_accessors( @_ ) if @_;
    
    return $obj;
}

=head2 $bool = $object->mk_accessors( @ACCESSORS | \%ACCESSOR_MAP );

Creates a list of accessors for this object (and C<NOT> for other ones
in the same class!).
Will not clobber existing data, so if an accessor already exists,
requesting to create again is effectively a C<no-op>.

When providing a C<hashref> as argument, rather than a normal list,
you can specify a list of key/value pairs of accessors and their
respective input validators. The validators can be anything that
C<Params::Check>'s C<allow> function accepts. Please see its manpage
for details.

For example:

    $object->mk_accessors( {
        foo     => qr/^\d+$/,       # digits only
        bar     => [0,1],           # booleans
        zot     => \&my_sub         # a custom verification sub
    } );        

Returns true on success, false on failure.

Accessors that are called on an object, that do not exist return
C<undef> by default, but you can make this a fatal error by setting the
global variable C<$FATAL> to true. See the section on C<GLOBAL
VARIABLES> for details.

Note that you can bind the values of attributes to a scope. This allows
you to C<temporarily> change a value of an attribute, and have it's 
original value restored up on the end of it's bound variable's scope;

For example, in this snippet of code, the attribute C<foo> will 
temporarily be set to C<2>, until the end of the scope of C<$x>, at 
which point the original value of C<1> will be restored.

    my $obj = Object::Accessor->new;
    
    $obj->mk_accessors('foo');
    $obj->foo( 1 );
    print $obj->foo;                # will print 1

    ### bind the scope of the value of attribute 'foo'
    ### to the scope of '$x' -- when $x goes out of 
    ### scope, 'foo' previous value will be restored
    {   $obj->foo( 2 => \my $x );
        print $obj->foo, ' ', $x;   # will print '2 2'
    }
    print $obj->foo;                # will print 1
    

Note that all accessors are read/write for everyone. See the C<TODO>
section for details.

=cut

sub mk_accessors {
    my $self    = $_[0];
    my $is_hash = UNIVERSAL::isa( $_[1], 'HASH' );
    
    ### first argument is a hashref, which means key/val pairs
    ### as keys + allow handlers
    for my $acc ( $is_hash ? keys %{$_[1]} : @_[1..$#_] ) {
    
        ### already created apparently
        if( exists $self->{$acc} ) {
            __PACKAGE__->___debug( "Accessor '$acc' already exists");
            next;
        }

        __PACKAGE__->___debug( "Creating accessor '$acc'");

        ### explicitly vivify it, so that exists works in ls_accessors()
        $self->{$acc}->[VALUE] = undef;
        
        ### set the allow handler only if one was specified
        $self->{$acc}->[ALLOW] = $_[1]->{$acc} if $is_hash;
    }

    return 1;
}

=head2 @list = $self->ls_accessors;

Returns a list of accessors that are supported by the current object.
The corresponding coderefs can be retrieved by passing this list one
by one to the C<can> method.

=cut

sub ls_accessors {
    ### metainformation is stored in the stringified 
    ### key of the object, so skip that when listing accessors
    return sort grep { $_ ne "$_[0]" } keys %{$_[0]};
}

=head2 $ref = $self->ls_allow(KEY)

Returns the allow handler for the given key, which can be used with
C<Params::Check>'s C<allow()> handler. If there was no allow handler
specified, an allow handler that always returns true will be returned.

=cut

sub ls_allow {
    my $self = shift;
    my $key  = shift or return;
    return exists $self->{$key}->[ALLOW]
                ? $self->{$key}->[ALLOW] 
                : sub { 1 };
}

=head2 $bool = $self->mk_aliases( alias => method, [alias2 => method2, ...] );

Creates an alias for a given method name. For all intents and purposes,
these two accessors are now identical for this object. This is akin to
doing the following on the symbol table level:

  *alias = *method

This allows you to do the following:

  $self->mk_accessors('foo');
  $self->mk_aliases( bar => 'foo' );
  
  $self->bar( 42 );
  print $self->foo;     # will print 42

=cut

sub mk_aliases {
    my $self    = shift;
    my %aliases = @_;
    
    while( my($alias, $method) = each %aliases ) {

        ### already created apparently
        if( exists $self->{$alias} ) {
            __PACKAGE__->___debug( "Accessor '$alias' already exists");
            next;
        }

        $self->___alias( $alias => $method );
    }

    return 1;
}

=head2 $clone = $self->mk_clone;

Makes a clone of the current object, which will have the exact same
accessors as the current object, but without the data stored in them.

=cut

### XXX this creates an object WITH allow handlers at all times.
### even if the original didnt
sub mk_clone {
    my $self    = $_[0];
    my $class   = ref $self;

    my $clone   = $class->new;
    
    ### split out accessors with and without allow handlers, so we
    ### don't install dummy allow handers (which makes O::A::lvalue
    ### warn for example)
    my %hash; my @list;
    for my $acc ( $self->ls_accessors ) {
        my $allow = $self->{$acc}->[ALLOW];
        $allow ? $hash{$acc} = $allow : push @list, $acc;

        ### is this an alias?
        if( my $org = $self->{ $acc }->[ ALIAS ] ) {
            $clone->___alias( $acc => $org );
        }
    }

    ### copy the accessors from $self to $clone
    $clone->mk_accessors( \%hash ) if %hash;
    $clone->mk_accessors( @list  ) if @list;

    ### copy callbacks
    #$clone->{"$clone"} = $self->{"$self"} if $self->{"$self"};
    $clone->___callback( $self->___callback );

    return $clone;
}

=head2 $bool = $self->mk_flush;

Flushes all the data from the current object; all accessors will be
set back to their default state of C<undef>.

Returns true on success and false on failure.

=cut

sub mk_flush {
    my $self = $_[0];

    # set each accessor's data to undef
    $self->{$_}->[VALUE] = undef for $self->ls_accessors;

    return 1;
}

=head2 $bool = $self->mk_verify;

Checks if all values in the current object are in accordance with their
own allow handler. Specifically useful to check if an empty initialised
object has been filled with values satisfying their own allow criteria.

=cut

sub mk_verify {
    my $self = $_[0];
    
    my $fail;
    for my $name ( $self->ls_accessors ) {
        unless( allow( $self->$name, $self->ls_allow( $name ) ) ) {
            my $val = defined $self->$name ? $self->$name : '<undef>';

            __PACKAGE__->___error("'$name' ($val) is invalid");
            $fail++;
        }
    }

    return if $fail;
    return 1;
}   

=head2 $bool = $self->register_callback( sub { ... } );

This method allows you to register a callback, that is invoked
every time an accessor is called. This allows you to munge input
data, access external data stores, etc.

You are free to return whatever you wish. On a C<set> call, the
data is even stored in the object.

Below is an example of the use of a callback.
    
    $object->some_method( "some_value" );
    
    my $callback = sub {
        my $self    = shift; # the object
        my $meth    = shift; # "some_method"
        my $val     = shift; # ["some_value"]  
                             # could be undef -- check 'exists';
                             # if scalar @$val is empty, it was a 'get'
    
        # your code here

        return $new_val;     # the value you want to be set/returned
    }        

To access the values stored in the object, circumventing the
callback structure, you should use the C<___get> and C<___set> methods
documented further down. 

=cut

sub register_callback {
    my $self    = shift;
    my $sub     = shift or return;
    
    ### use the memory address as key, it's not used EVER as an
    ### accessor --kane
    $self->___callback( $sub );

    return 1;
}


=head2 $bool = $self->can( METHOD_NAME )

This method overrides C<UNIVERAL::can> in order to provide coderefs to
accessors which are loaded on demand. It will behave just like
C<UNIVERSAL::can> where it can -- returning a class method if it exists,
or a closure pointing to a valid accessor of this particular object.

You can use it as follows:

    $sub = $object->can('some_accessor');   # retrieve the coderef
    $sub->('foo');                          # 'some_accessor' now set
                                            # to 'foo' for $object
    $foo = $sub->();                        # retrieve the contents
                                            # of 'some_accessor'

See the C<SYNOPSIS> for more examples.

=cut

### custom 'can' as UNIVERSAL::can ignores autoload
sub can {
    my($self, $method) = @_;

    ### it's one of our regular methods
    if( $self->UNIVERSAL::can($method) ) {
        __PACKAGE__->___debug( "Can '$method' -- provided by package" );
        return $self->UNIVERSAL::can($method);
    }

    ### it's an accessor we provide;
    if( UNIVERSAL::isa( $self, 'HASH' ) and exists $self->{$method} ) {
        __PACKAGE__->___debug( "Can '$method' -- provided by object" );
        return sub { $self->$method(@_); }
    }

    ### we don't support it
    __PACKAGE__->___debug( "Cannot '$method'" );
    return;
}

### don't autoload this
sub DESTROY { 1 };

### use autoload so we can have per-object accessors,
### not per class, as that is incorrect
sub AUTOLOAD {
    my $self    = shift;
    my($method) = ($AUTOLOAD =~ /([^:']+$)/);

    my $val = $self->___autoload( $method, @_ ) or return;

    return $val->[0];
}

sub ___autoload {
    my $self    = shift;
    my $method  = shift;
    my $assign  = scalar @_;    # is this an assignment?

    ### a method on our object
    if( UNIVERSAL::isa( $self, 'HASH' ) ) {
        if ( not exists $self->{$method} ) {
            __PACKAGE__->___error("No such accessor '$method'", 1);
            return;
        } 
   
    ### a method on something else, die with a descriptive error;
    } else {     
        local $FATAL = 1;
        __PACKAGE__->___error( 
                "You called '$AUTOLOAD' on '$self' which was interpreted by ".
                __PACKAGE__ . " as an object call. Did you mean to include ".
                "'$method' from somewhere else?", 1 );
    }        

    ### is this is an alias, redispatch to the original method
    if( my $original = $self->{ $method }->[ALIAS] ) {
        return $self->___autoload( $original, @_ );
    }        

    ### assign?
    my $val = $assign ? shift(@_) : $self->___get( $method );

    if( $assign ) {

        ### any binding?
        if( $_[0] ) {
            if( ref $_[0] and UNIVERSAL::isa( $_[0], 'SCALAR' ) ) {
            
                ### tie the reference, so we get an object and
                ### we can use it's going out of scope to restore
                ### the old value
                my $cur = $self->{$method}->[VALUE];
                
                tie ${$_[0]}, __PACKAGE__ . '::TIE', 
                        sub { $self->$method( $cur ) };
    
                ${$_[0]} = $val;
            
            } else {
                __PACKAGE__->___error( 
                    "Can not bind '$method' to anything but a SCALAR", 1 
                );
            }
        }
        
        ### need to check the value?
        if( exists $self->{$method}->[ALLOW] ) {

            ### double assignment due to 'used only once' warnings
            local $Params::Check::VERBOSE = 0;
            local $Params::Check::VERBOSE = 0;
            
            allow( $val, $self->{$method}->[ALLOW] ) or (
                __PACKAGE__->___error( 
                    "'$val' is an invalid value for '$method'", 1), 
                return 
            ); 
        }
    }
    
    ### callbacks?
    if( my $sub = $self->___callback ) {
        $val = eval { $sub->( $self, $method, ($assign ? [$val] : []) ) };
        
        ### register the error
        $self->___error( $@, 1 ), return if $@;
    }

    ### now we can actually assign it
    if( $assign ) {
        $self->___set( $method, $val ) or return;
    }
    
    return [$val];
}

=head2 $val = $self->___get( METHOD_NAME );

Method to directly access the value of the given accessor in the
object. It circumvents all calls to allow checks, callbakcs, etc.

Use only if you C<Know What You Are Doing>! General usage for 
this functionality would be in your own custom callbacks.

=cut

### XXX O::A::lvalue is mirroring this behaviour! if this
### changes, lvalue's autoload must be changed as well
sub ___get {
    my $self    = shift;
    my $method  = shift or return;
    return $self->{$method}->[VALUE];
}

=head2 $bool = $self->___set( METHOD_NAME => VALUE );

Method to directly set the value of the given accessor in the
object. It circumvents all calls to allow checks, callbakcs, etc.

Use only if you C<Know What You Are Doing>! General usage for 
this functionality would be in your own custom callbacks.

=cut 

sub ___set {
    my $self    = shift;
    my $method  = shift or return;
   
    ### you didn't give us a value to set!
    exists $_[0] or return;
    my $val     = shift;
 
    ### if there's more arguments than $self, then
    ### replace the method called by the accessor.
    ### XXX implement rw vs ro accessors!
    $self->{$method}->[VALUE] = $val;

    return 1;
}

=head2 $bool = $self->___alias( ALIAS => METHOD );

Method to directly alias one accessor to another for
this object. It circumvents all sanity checks, etc.

Use only if you C<Know What You Are Doing>! 

=cut

sub ___alias {
    my $self    = shift;
    my $alias   = shift or return;
    my $method  = shift or return;
    
    $self->{ $alias }->[ALIAS] = $method;
    
    return 1;
}

sub ___debug {
    return unless $DEBUG;

    my $self = shift;
    my $msg  = shift;
    my $lvl  = shift || 0;

    local $Carp::CarpLevel += 1;
    
    carp($msg);
}

sub ___error {
    my $self = shift;
    my $msg  = shift;
    my $lvl  = shift || 0;
    local $Carp::CarpLevel += ($lvl + 1);
    $FATAL ? croak($msg) : carp($msg);
}

### objects might be overloaded.. if so, we can't trust what "$self"
### will return, which might get *really* painful.. so check for that
### and get their unoverloaded stringval if needed.
sub ___callback {
    my $self = shift;
    my $sub  = shift;
    
    my $mem  = overload::Overloaded( $self )
                ? overload::StrVal( $self )
                : "$self";

    $self->{$mem} = $sub if $sub;
    
    return $self->{$mem};
}

=head1 LVALUE ACCESSORS

C<Object::Accessor> supports C<lvalue> attributes as well. To enable
these, you should create your objects in the designated namespace,
C<Object::Accessor::Lvalue>. For example:

    my $obj = Object::Accessor::Lvalue->new('foo');
    $obj->foo += 1;
    print $obj->foo;
    
will actually print C<1> and work as expected. Since this is an
optional feature, that's not desirable in all cases, we require
you to explicitly use the C<Object::Accessor::Lvalue> class.

Doing the same on the standard C<Object>>Accessor> class would
generate the following code & errors:

    my $obj = Object::Accessor->new('foo');
    $obj->foo += 1;

    Can't modify non-lvalue subroutine call

Note that C<lvalue> support on C<AUTOLOAD> routines is a
C<perl 5.8.x> feature. See perldoc L<perl58delta> for details.

=head2 CAVEATS

=over 4

=item * Allow handlers

Due to the nature of C<lvalue subs>, we never get access to the
value you are assigning, so we can not check it againt your allow
handler. Allow handlers are therefor unsupported under C<lvalue>
conditions.

See C<perldoc perlsub> for details.

=item * Callbacks

Due to the nature of C<lvalue subs>, we never get access to the
value you are assigning, so we can not check provide this value
to your callback. Furthermore, we can not distinguish between
a C<get> and a C<set> call. Callbacks are therefor unsupported 
under C<lvalue> conditions.

See C<perldoc perlsub> for details.


=cut

{   package Object::Accessor::Lvalue;
    use base 'Object::Accessor';
    use strict;
    use vars qw[$AUTOLOAD];

    ### constants needed to access values from the objects
    *VALUE = *Object::Accessor::VALUE;
    *ALLOW = *Object::Accessor::ALLOW;

    ### largely copied from O::A::Autoload 
    sub AUTOLOAD : lvalue {
        my $self    = shift;
        my($method) = ($AUTOLOAD =~ /([^:']+$)/);

        $self->___autoload( $method, @_ ) or return;

        ### *dont* add return to it, or it won't be stored
        ### see perldoc perlsub on lvalue subs
        ### XXX can't use $self->___get( ... ), as we MUST have
        ### the container that's used for the lvalue assign as
        ### the last statement... :(
        $self->{$method}->[ VALUE() ];
    }

    sub mk_accessors {
        my $self    = shift;
        my $is_hash = UNIVERSAL::isa( $_[0], 'HASH' );
        
        $self->___error(
            "Allow handlers are not supported for '". __PACKAGE__ ."' objects"
        ) if $is_hash;
        
        return $self->SUPER::mk_accessors( @_ );
    }                    
    
    sub register_callback {
        my $self = shift;
        $self->___error(
            "Callbacks are not supported for '". __PACKAGE__ ."' objects"
        );
        return;
    }        
}    


### standard tie class for bound attributes
{   package Object::Accessor::TIE;
    use Tie::Scalar;
    use Data::Dumper;
    use base 'Tie::StdScalar';

    my %local = ();

    sub TIESCALAR {
        my $class   = shift;
        my $sub     = shift;
        my $ref     = undef;
        my $obj     =  bless \$ref, $class;

        ### store the restore sub 
        $local{ $obj } = $sub;
        return $obj;
    }
    
    sub DESTROY {
        my $tied    = shift;
        my $sub     = delete $local{ $tied };

        ### run the restore sub to set the old value back
        return $sub->();        
    }              
}

=back

=head1 GLOBAL VARIABLES

=head2 $Object::Accessor::FATAL

Set this variable to true to make all attempted access to non-existant
accessors be fatal.
This defaults to C<false>.

=head2 $Object::Accessor::DEBUG

Set this variable to enable debugging output.
This defaults to C<false>.

=head1 TODO

=head2 Create read-only accessors

Currently all accessors are read/write for everyone. Perhaps a future
release should make it possible to have read-only accessors as well.

=head1 CAVEATS

If you use codereferences for your allow handlers, you will not be able
to freeze the data structures using C<Storable>.

Due to a bug in storable (until at least version 2.15), C<qr//> compiled 
regexes also don't de-serialize properly. Although this bug has been 
reported, you should be aware of this issue when serializing your objects.

You can track the bug here: 

    http://rt.cpan.org/Ticket/Display.html?id=1827

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-object-accessor@rt.cpan.orgE<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.

=cut

1;
