package Module::Pluggable;

use strict;
use vars qw($VERSION);
use Module::Pluggable::Object;

# ObQuote:
# Bob Porter: Looks like you've been missing a lot of work lately. 
# Peter Gibbons: I wouldn't say I've been missing it, Bob! 


$VERSION = '3.8';

sub import {
    my $class        = shift;
    my %opts         = @_;

    my ($pkg, $file) = caller; 
    # the default name for the method is 'plugins'
    my $sub          = $opts{'sub_name'}  || 'plugins';
    # get our package 
    my ($package)    = $opts{'package'} || $pkg;
    $opts{filename}  = $file;
    $opts{package}   = $package;


    my $finder       = Module::Pluggable::Object->new(%opts);
    my $subroutine   = sub { my $self = shift; return $finder->plugins(@_) };

    my $searchsub = sub {
              my $self = shift;
              my ($action,@paths) = @_;

              $finder->{'search_path'} = ["${package}::Plugin"] if ($action eq 'add'  and not   $finder->{'search_path'} );
              push @{$finder->{'search_path'}}, @paths      if ($action eq 'add');
              $finder->{'search_path'}       = \@paths      if ($action eq 'new');
              return $finder->{'search_path'};
    };


    my $onlysub = sub {
        my ($self, $only) = @_;

        if (defined $only) {
            $finder->{'only'} = $only;
        };
        
        return $finder->{'only'};
    };

    my $exceptsub = sub {
        my ($self, $except) = @_;

        if (defined $except) {
            $finder->{'except'} = $except;
        };
        
        return $finder->{'except'};
    };


    no strict 'refs';
    no warnings qw(redefine prototype);
    
    *{"$package\::$sub"}        = $subroutine;
    *{"$package\::search_path"} = $searchsub;
    *{"$package\::only"}        = $onlysub;
    *{"$package\::except"}      = $exceptsub;

}

1;

=pod

=head1 NAME

Module::Pluggable - automatically give your module the ability to have plugins

=head1 SYNOPSIS


Simple use Module::Pluggable -

    package MyClass;
    use Module::Pluggable;
    

and then later ...

    use MyClass;
    my $mc = MyClass->new();
    # returns the names of all plugins installed under MyClass::Plugin::*
    my @plugins = $mc->plugins(); 

=head1 EXAMPLE

Why would you want to do this? Say you have something that wants to pass an
object to a number of different plugins in turn. For example you may 
want to extract meta-data from every email you get sent and do something
with it. Plugins make sense here because then you can keep adding new 
meta data parsers and all the logic and docs for each one will be 
self contained and new handlers are easy to add without changing the 
core code. For that, you might do something like ...

    package Email::Examiner;

    use strict;
    use Email::Simple;
    use Module::Pluggable require => 1;

    sub handle_email {
        my $self  = shift;
        my $email = shift;

        foreach my $plugin ($self->plugins) {
            $plugin->examine($email);
        }

        return 1;
    }



.. and all the plugins will get a chance in turn to look at it.

This can be trivally extended so that plugins could save the email
somewhere and then no other plugin should try and do that. 
Simply have it so that the C<examine> method returns C<1> if 
it has saved the email somewhere. You might also wnat to be paranoid
and check to see if the plugin has an C<examine> method.

        foreach my $plugin ($self->plugins) {
            next unless $plugin->can('examine');
            last if     $plugin->examine($email);
        }


And so on. The sky's the limit.


=head1 DESCRIPTION

Provides a simple but, hopefully, extensible way of having 'plugins' for 
your module. Obviously this isn't going to be the be all and end all of
solutions but it works for me.

Essentially all it does is export a method into your namespace that 
looks through a search path for .pm files and turn those into class names. 

Optionally it instantiates those classes for you.

=head1 ADVANCED USAGE

    
Alternatively, if you don't want to use 'plugins' as the method ...
    
    package MyClass;
    use Module::Pluggable sub_name => 'foo';


and then later ...

    my @plugins = $mc->foo();


Or if you want to look in another namespace

    package MyClass;
    use Module::Pluggable search_path => ['Acme::MyClass::Plugin', 'MyClass::Extend'];

or directory 

    use Module::Pluggable search_dirs => ['mylibs/Foo'];


Or if you want to instantiate each plugin rather than just return the name

    package MyClass;
    use Module::Pluggable instantiate => 'new';

and then

    # whatever is passed to 'plugins' will be passed 
    # to 'new' for each plugin 
    my @plugins = $mc->plugins(@options); 


alternatively you can just require the module without instantiating it

    package MyClass;
    use Module::Pluggable require => 1;

since requiring automatically searches inner packages, which may not be desirable, you can turn this off


    package MyClass;
    use Module::Pluggable require => 1, inner => 0;


You can limit the plugins loaded using the except option, either as a string,
array ref or regex

    package MyClass;
    use Module::Pluggable except => 'MyClass::Plugin::Foo';

or

    package MyClass;
    use Module::Pluggable except => ['MyClass::Plugin::Foo', 'MyClass::Plugin::Bar'];

or

    package MyClass;
    use Module::Pluggable except => qr/^MyClass::Plugin::(Foo|Bar)$/;


and similarly for only which will only load plugins which match.

Remember you can use the module more than once

    package MyClass;
    use Module::Pluggable search_path => 'MyClass::Filters' sub_name => 'filters';
    use Module::Pluggable search_path => 'MyClass::Plugins' sub_name => 'plugins';

and then later ...

    my @filters = $self->filters;
    my @plugins = $self->plugins;

=head1 INNER PACKAGES

If you have, for example, a file B<lib/Something/Plugin/Foo.pm> that
contains package definitions for both C<Something::Plugin::Foo> and 
C<Something::Plugin::Bar> then as long as you either have either 
the B<require> or B<instantiate> option set then we'll also find 
C<Something::Plugin::Bar>. Nifty!

=head1 OPTIONS

You can pass a hash of options when importing this module.

The options can be ...

=head2 sub_name

The name of the subroutine to create in your namespace. 

By default this is 'plugins'

=head2 search_path

An array ref of namespaces to look in. 

=head2 search_dirs 

An array ref of directorys to look in before @INC.

=head2 instantiate

Call this method on the class. In general this will probably be 'new'
but it can be whatever you want. Whatever arguments are passed to 'plugins' 
will be passed to the method.

The default is 'undef' i.e just return the class name.

=head2 require

Just require the class, don't instantiate (overrides 'instantiate');

=head2 inner

If set to 0 will B<not> search inner packages. 
If set to 1 will override C<require>.

=head2 only

Takes a string, array ref or regex describing the names of the only plugins to 
return. Whilst this may seem perverse ... well, it is. But it also 
makes sense. Trust me.

=head2 except

Similar to C<only> it takes a description of plugins to exclude 
from returning. This is slightly less perverse.

=head2 package

This is for use by extension modules which build on C<Module::Pluggable>:
passing a C<package> option allows you to place the plugin method in a
different package other than your own.

=head2 file_regex

By default C<Module::Pluggable> only looks for I<.pm> files.

By supplying a new C<file_regex> then you can change this behaviour e.g

    file_regex => qr/\.plugin$/

=head2 include_editor_junk

By default C<Module::Pluggable> ignores files that look like they were
left behind by editors. Currently this means files ending in F<~> (~),
the extensions F<.swp> or F<.swo>, or files beginning with F<.#>.

Setting C<include_editor_junk> changes C<Module::Pluggable> so it does
not ignore any files it finds.


=head1 METHODs

=head2 search_path

The method C<search_path> is exported into you namespace as well. 
You can call that at any time to change or replace the 
search_path.

    $self->search_path( add => "New::Path" ); # add
    $self->search_path( new => "New::Path" ); # replace



=head1 FUTURE PLANS

This does everything I need and I can't really think of any other 
features I want to add. Famous last words of course

Recently tried fixed to find inner packages and to make it 
'just work' with PAR but there are still some issues.


However suggestions (and patches) are welcome.

=head1 AUTHOR

Simon Wistow <simon@thegestalt.org>

=head1 COPYING

Copyright, 2006 Simon Wistow

Distributed under the same terms as Perl itself.

=head1 BUGS

None known.

=head1 SEE ALSO

L<File::Spec>, L<File::Find>, L<File::Basename>, L<Class::Factory::Util>, L<Module::Pluggable::Ordered>

=cut 


