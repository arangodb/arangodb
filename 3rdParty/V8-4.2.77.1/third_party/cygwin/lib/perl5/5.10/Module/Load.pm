package Module::Load;

$VERSION = '0.12';

use strict;
use File::Spec ();

sub import {
    my $who = _who();

    {   no strict 'refs';
        *{"${who}::load"} = *load;
    }
}

sub load (*;@)  {
    my $mod = shift or return;
    my $who = _who();

    if( _is_file( $mod ) ) {
        require $mod;
    } else {
        LOAD: {
            my $err;
            for my $flag ( qw[1 0] ) {
                my $file = _to_file( $mod, $flag);
                eval { require $file };
                $@ ? $err .= $@ : last LOAD;
            }
            die $err if $err;
        }
    }
    __PACKAGE__->_export_to_level(1, $mod, @_) if @_;
}

### 5.004's Exporter doesn't have export_to_level.
### Taken from Michael Schwerns Test::More and slightly modified
sub _export_to_level {
    my $pkg     = shift;
    my $level   = shift;
    my $mod     = shift;
    my $callpkg = caller($level);

    $mod->export($callpkg, @_);
}

sub _to_file{
    local $_    = shift;
    my $pm      = shift || '';

    my @parts = split /::/;

    ### because of [perl #19213], see caveats ###
    my $file = $^O eq 'MSWin32'
                    ? join "/", @parts
                    : File::Spec->catfile( @parts );

    $file   .= '.pm' if $pm;
    
    ### on perl's before 5.10 (5.9.5@31746) if you require
    ### a file in VMS format, it's stored in %INC in VMS
    ### format. Therefor, better unixify it first
    ### Patch in reply to John Malmbergs patch (as mentioned
    ### above) on p5p Tue 21 Aug 2007 04:55:07
    $file = VMS::Filespec::unixify($file) if $^O eq 'VMS';

    return $file;
}

sub _who { (caller(1))[0] }

sub _is_file {
    local $_ = shift;
    return  /^\./               ? 1 :
            /[^\w:']/           ? 1 :
            undef
    #' silly bbedit..
}


1;

__END__

=pod

=head1 NAME

Module::Load - runtime require of both modules and files

=head1 SYNOPSIS

	use Module::Load;

    my $module = 'Data:Dumper';
    load Data::Dumper;      # loads that module
    load 'Data::Dumper';    # ditto
    load $module            # tritto
    
    my $script = 'some/script.pl'
    load $script;
    load 'some/script.pl';	# use quotes because of punctuations
    
    load thing;             # try 'thing' first, then 'thing.pm'

    load CGI, ':standard'   # like 'use CGI qw[:standard]'
    

=head1 DESCRIPTION

C<load> eliminates the need to know whether you are trying to require
either a file or a module.

If you consult C<perldoc -f require> you will see that C<require> will
behave differently when given a bareword or a string.

In the case of a string, C<require> assumes you are wanting to load a
file. But in the case of a bareword, it assumes you mean a module.

This gives nasty overhead when you are trying to dynamically require
modules at runtime, since you will need to change the module notation
(C<Acme::Comment>) to a file notation fitting the particular platform
you are on.

C<load> eliminates the need for this overhead and will just DWYM.

=head1 Rules

C<load> has the following rules to decide what it thinks you want:

=over 4

=item *

If the argument has any characters in it other than those matching
C<\w>, C<:> or C<'>, it must be a file

=item *

If the argument matches only C<[\w:']>, it must be a module

=item *

If the argument matches only C<\w>, it could either be a module or a
file. We will try to find C<file> first in C<@INC> and if that fails,
we will try to find C<file.pm> in @INC.
If both fail, we die with the respective error messages.

=back

=head1 Caveats

Because of a bug in perl (#19213), at least in version 5.6.1, we have
to hardcode the path separator for a require on Win32 to be C</>, like
on Unix rather than the Win32 C<\>. Otherwise perl will not read its
own %INC accurately double load files if they are required again, or
in the worst case, core dump.

C<Module::Load> cannot do implicit imports, only explicit imports.
(in other words, you always have to specify explicitly what you wish
to import from a module, even if the functions are in that modules'
C<@EXPORT>)

=head1 ACKNOWLEDGEMENTS

Thanks to Jonas B. Nielsen for making explicit imports work.

=head1 BUG REPORTS

Please report bugs or other issues to E<lt>bug-module-load@rt.cpan.org<gt>.

=head1 AUTHOR

This module by Jos Boumans E<lt>kane@cpan.orgE<gt>.

=head1 COPYRIGHT

This library is free software; you may redistribute and/or modify it 
under the same terms as Perl itself.


=cut                               
