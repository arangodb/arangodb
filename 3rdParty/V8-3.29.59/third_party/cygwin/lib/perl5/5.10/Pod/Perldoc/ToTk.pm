
require 5;
package Pod::Perldoc::ToTk;
use strict;
use warnings;

use base qw(Pod::Perldoc::BaseTo);

sub is_pageable        { 1 }
sub write_with_binmode { 0 }
sub output_extension   { 'txt' } # doesn't matter
sub if_zero_length { }  # because it will be 0-length!
sub new { return bless {}, ref($_[0]) || $_[0] }

# TODO: document these and their meanings...
sub tree      { shift->_perldoc_elem('tree'    , @_) }
sub tk_opt    { shift->_perldoc_elem('tk_opt'  , @_) }
sub forky     { shift->_perldoc_elem('forky'   , @_) }

use Pod::Perldoc ();
use File::Spec::Functions qw(catfile);

use Tk;
die join '', __PACKAGE__, " doesn't work nice with Tk.pm verison $Tk::VERSION"
 if $Tk::VERSION eq '800.003';

BEGIN { eval { require Tk::FcyEntry; }; };
use Tk::Pod;

# The following was adapted from "tkpod" in the Tk-Pod dist.

sub parse_from_file {

    my($self, $Input_File) = @_;
    if($self->{'forky'}) {
      return if fork;  # i.e., parent process returns
    }
    
    $Input_File =~ s{\\}{/}g
     if Pod::Perldoc::IS_MSWin32 or Pod::Perldoc::IS_Dos
     # and maybe OS/2
    ;
    
    my($tk_opt, $tree);
    $tree   = $self->{'tree'  };
    $tk_opt = $self->{'tk_opt'};
    
    #require Tk::ErrorDialog;
    
    # Add 'Tk' subdirectories to search path so, e.g.,
    # 'Scrolled' will find doc in 'Tk/Scrolled'
    
    if( $tk_opt ) {
      push @INC, grep -d $_, map catfile($_,'Tk'), @INC;
    }
    
    my $mw = MainWindow->new();
    #eval 'use blib "/home/e/eserte/src/perl/Tk-App";require Tk::App::Debug';
    $mw->withdraw;
    
    # CDE use Font Settings if available
    my $ufont = $mw->optionGet('userFont','UserFont');     # fixed width
    my $sfont = $mw->optionGet('systemFont','SystemFont'); # proportional
    if (defined($ufont) and defined($sfont)) {
        foreach ($ufont, $sfont) { s/:$//; };
        $mw->optionAdd('*Font',       $sfont);
        $mw->optionAdd('*Entry.Font', $ufont);
        $mw->optionAdd('*Text.Font',  $ufont);
    }
    
    $mw->optionAdd('*Menu.tearOff', $Tk::platform ne 'MSWin32' ? 1 : 0);
    
    $mw->Pod(
      '-file' => $Input_File,
      (($Tk::Pod::VERSION >= 4) ? ('-tree' => $tree) : ())
    )->focusNext;
    
    # xxx dirty but it works. A simple $mw->destroy if $mw->children
    # does not work because Tk::ErrorDialogs could be created.
    # (they are withdrawn after Ok instead of destory'ed I guess)
    
    if ($mw->children) {
        $mw->repeat(1000, sub {
                    # ErrorDialog is withdrawn not deleted :-(
                    foreach ($mw->children) {
                            return if "$_" =~ /^Tk::Pod/  # ->isa('Tk::Pod')
                    }
                    $mw->destroy;
                });
    } else {
        $mw->destroy;
    }
    #$mw->WidgetDump;
    MainLoop();

    exit if $self->{'forky'}; # we were the child!  so exit now!
    return;
}

1;
__END__


=head1 NAME

Pod::Perldoc::ToTk - let Perldoc use Tk::Pod to render Pod

=head1 SYNOPSIS

  perldoc -o tk Some::Modulename &

=head1 DESCRIPTION

This is a "plug-in" class that allows Perldoc to use
Tk::Pod as a formatter class.

You have to have installed Tk::Pod first, or this class won't load.

=head1 SEE ALSO

L<Tk::Pod>, L<Pod::Perldoc>

=head1 AUTHOR

Sean M. Burke C<sburke@cpan.org>, with significant portions copied from
F<tkpod> in the Tk::Pod dist, by Nick Ing-Simmons, Slaven Rezic, et al.

=cut

