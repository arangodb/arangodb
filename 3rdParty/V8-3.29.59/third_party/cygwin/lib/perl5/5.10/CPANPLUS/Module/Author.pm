package CPANPLUS::Module::Author;

use strict;

use CPANPLUS::Error;
use Params::Check               qw[check];
use Locale::Maketext::Simple    Class => 'CPANPLUS', Style => 'gettext';

local $Params::Check::VERBOSE = 1;

=pod

=head1 NAME

CPANPLUS::Module::Author

=head1 SYNOPSIS

    my $author = CPANPLUS::Module::Author->new(
                    author  => 'Jack Ashton',
                    cpanid  => 'JACKASH',
                    _id     => INTERNALS_OBJECT_ID,
                );

    $author->cpanid;
    $author->author;
    $author->email;

    @dists  = $author->distributions;
    @mods   = $author->modules;

    @accessors = CPANPLUS::Module::Author->accessors;

=head1 DESCRIPTION

C<CPANPLUS::Module::Author> creates objects from the information in the
source files. These can then be used to query on.

These objects should only be created internally. For C<fake> objects,
there's the C<CPANPLUS::Module::Author::Fake> class.

=head1 ACCESSORS

An objects of this class has the following accessors:

=over 4

=item author

Name of the author.

=item cpanid

The CPAN id of the author.

=item email

The email address of the author, which defaults to '' if not provided.

=item parent

The C<CPANPLUS::Internals::Object> that spawned this module object.

=back

=cut

my $tmpl = {
    author      => { required => 1 },   # full name of the author
    cpanid      => { required => 1 },   # cpan id
    email       => { default => '' },   # email address of the author
    _id         => { required => 1 },   # id of the Internals object that spawned us
};

### autogenerate accessors ###
for my $key ( keys %$tmpl ) {
    no strict 'refs';
    *{__PACKAGE__."::$key"} = sub {
        my $self = shift;
        $self->{$key} = $_[0] if @_;
        return $self->{$key};
    }
}

sub parent {
    my $self = shift;
    my $obj  = CPANPLUS::Internals->_retrieve_id( $self->_id );

    return $obj;
}

=pod

=head1 METHODS

=head2 $auth = CPANPLUS::Module::Author->new( author => AUTHOR_NAME, cpanid => CPAN_ID, _id => INTERNALS_ID [, email => AUTHOR_EMAIL] )

This method returns a C<CPANPLUS::Module::Author> object, based on the given
parameters.

Returns false on failure.

=cut

sub new {
    my $class   = shift;
    my %hash    = @_;

    ### don't check the template for sanity
    ### -- we know it's good and saves a lot of performance
    local $Params::Check::SANITY_CHECK_TEMPLATE = 0;

    my $object = check( $tmpl, \%hash ) or return;

    return bless $object, $class;
}

=pod

=head2 @mod_objs = $auth->modules()

Return a list of module objects this author has released.

=cut

sub modules {
    my $self    = shift;
    my $cb      = $self->parent;

    my $aref = $cb->_search_module_tree(
                    type    => 'author',
                    allow   => [$self],
                );
    return @$aref if $aref;
    return;
}

=pod

=head2 @dists = $auth->distributions()

Returns a list of module objects representing all the distributions
this author has released.

=cut

sub distributions {
    my $self = shift;
    my %hash = @_;

    local $Params::Check::ALLOW_UNKNOWN = 1;
    local $Params::Check::NO_DUPLICATES = 1;

    my $mod;
    my $tmpl = {
        module  => { default => '', store => \$mod },
    };

    my $args = check( $tmpl, \%hash ) or return;

    ### if we didn't get a module object passed, we'll find one ourselves ###
    unless( $mod ) {
        my @list = $self->modules;
        if( @list ) {
            $mod = $list[0];
        } else {
            error( loc( "This author has released no modules" ) );
            return;
        }
    }

    my $file = $mod->checksums( %hash );
    my $href = $mod->_parse_checksums_file( file => $file ) or return;

    my @rv;
    for my $dist ( keys %$href ) {
        my $clone = $mod->clone;

        $clone->package( $dist );
        $clone->module(  $clone->package_name );
        $clone->version( $clone->package_version );
        $clone->mtime(   $href->{$dist}->{'mtime'} );   # release date

        ### .meta files are now also in the checksums file,
        ### which means we have to filter out things that dont
        ### match our regex
        push @rv, $clone if $clone->package_extension;
    }

    return @rv;
}


=pod

=head1 CLASS METHODS

=head2 accessors ()

Returns a list of all accessor methods to the object

=cut

sub accessors { return keys %$tmpl };

1;

# Local variables:
# c-indentation-style: bsd
# c-basic-offset: 4
# indent-tabs-mode: nil
# End:
# vim: expandtab shiftwidth=4:
