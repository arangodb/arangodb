package CPAN::Admin;
use base CPAN;
use CPAN; # old base.pm did not load CPAN on previous line
use strict;
use vars qw(@EXPORT $VERSION);
use constant PAUSE_IP => "pause.perl.org";

@EXPORT = qw(shell);
$VERSION = "5.5";
push @CPAN::Complete::COMMANDS, qw(register modsearch);
$CPAN::Shell::COLOR_REGISTERED = 1;

sub shell {
    CPAN::shell($_[0]||"admin's cpan> ",$_[1]);
}

sub CPAN::Shell::register {
    my($self,$mod,@rest) = @_;
    unless ($mod) {
        print "register called without argument\n";
        return;
    }
    if ($CPAN::META->has_inst("URI::Escape")) {
        require URI::Escape;
    } else {
        print "register requires URI::Escape installed, otherwise it cannot work\n";
        return;
    }
    print "Got request for mod[$mod]\n";
    if (@rest) {
        my $modline = join " ", $mod, @rest;
        print "Sending to PAUSE [$modline]\n";
        my $emodline = URI::Escape::uri_escape($modline, '^\w ');
        $emodline =~ s/ /+/g;
        my $url =
            sprintf("https://%s/pause/authenquery?pause99_add_mod_modid=".
                    "%s;SUBMIT_pause99_add_mod_hint=hint",
                    PAUSE_IP,
                    $emodline,
                   );
        print "url[$url]\n\n";
        print ">>>>Trying to open a netscape window<<<<\n";
        sleep 1;
        system("netscape","-remote","openURL($url)");
        return;
    }
    my $m = CPAN::Shell->expand("Module",$mod);
    unless (ref $m) {
        print "Could not determine the object for $mod\n";
        return;
    }
    my $id = $m->id;
    print "Found module id[$id] in database\n";

    if (exists $m->{RO} && $m->{RO}{chapterid}) {
        print "$id is already registered\n";
        return;
    }

    my(@namespace) = split /::/, $id;
    my $rootns = $namespace[0];

    # Tk, XML and Apache need special treatment
    if ($rootns=~/^(Bundle)\b/) {
        print "Bundles are not yet ready for registering\n";
        return;
    }

    # make a good suggestion for the chapter
    my(@simile) = CPAN::Shell->expand("Module","/^$rootns(:|\$)/");
    print "Found within this namespace ", join(", ", map { $_->id } @simile), "\n";
    my(%seench);
    for my $ch (map { exists $_->{RO} ? $_->{RO}{chapterid} : ""} @simile) {
        next unless $ch;
        $seench{$ch}=undef;
    }
    my(@seench) = sort grep {length($_)} keys %seench;
    my $reco_ch = "";
    if (@seench>1) {
        print "Found rootnamespace[$rootns] in the chapters [", join(", ", @seench), "]\n";
        $reco_ch = $seench[0];
        print "Picking $reco_ch\n";
    } elsif (@seench==1) {
        print "Found rootnamespace[$rootns] in the chapter[$seench[0]]\n";
        $reco_ch = $seench[0];
    } else {
        print "The new rootnamespace[$rootns] needs to be introduced. Oh well.\n";
    }

    # Look closer at the dist
    my $d = CPAN::Shell->expand("Distribution", $m->cpan_file);
    printf "Module comes with dist[%s]\n", $d->id;
    for my $contm ($d->containsmods) {
        if ($CPAN::META->exists("CPAN::Module",$contm)) {
            my $contm_obj = CPAN::Shell->expand("Module",$contm) or next;
            my $is_reg = exists $contm_obj->{RO} && $contm_obj->{RO}{description};
            printf(" in same dist: %s%s\n",
                   $contm,
                   $is_reg ? " already in modulelist" : "",
                  );
        }
    }

    # get it so that m is better and we can inspect for XS
    CPAN::Shell->get($id);
    CPAN::Shell->m($id);
    CPAN::Shell->d($d->id);

    my $has_xs = 0;
    {
        my($mani,@mani);
        local $/ = "\n";
        open $mani, "$d->{build_dir}/MANIFEST" and @mani = <$mani>;
        my @xs = grep /\.xs\b/, @mani;
        if (@xs) {
            print "Found XS files: @xs";
            $has_xs=1;
        }
    }
    my $emodid = URI::Escape::uri_escape($id, '\W');
    my $ech = $reco_ch;
    $ech =~ s/ /+/g;
    my $description = $m->{MANPAGE} || "";
    $description =~ s/[A-Z]<//; # POD markup (and maybe more)
    $description =~ s/^\s+//; # leading spaces
    $description =~ s/>//; # POD
    $description =~ s/^\Q$id\E//; # usually this line starts with the modid
    $description =~ s/^[ \-]+//; # leading spaces and dashes
    substr($description,44) = "" if length($description)>44;
    $description = ucfirst($description);
    my $edescription = URI::Escape::uri_escape($description, '^\w ');
    $edescription =~ s/ /+/g;
    my $url =
        sprintf("https://%s/pause/authenquery?pause99_add_mod_modid=".
                "%s;pause99_add_mod_chapterid=%s;pause99_add_mod_statd=%s;".
                "pause99_add_mod_stats=%s;pause99_add_mod_statl=%s;".
                "pause99_add_mod_stati=%s;pause99_add_mod_description=%s;".
                "pause99_add_mod_userid=%s;SUBMIT_pause99_add_mod_preview=preview",
                PAUSE_IP,
                $emodid,
                $ech,
                "R",
                "d",
                $has_xs ? "c" : "p",
                "O",
                $edescription,
                $m->{RO}{CPAN_USERID},
               );
    print "$url\n\n";
    print ">>>>Trying to open a netscape window<<<<\n";
    system("netscape","-remote","openURL($url)");
}

sub CPAN::Shell::modsearch {
    my($self,@line) = @_;
    unless (@line) {
        print "modsearch called without argument\n";
        return;
    }
    my $request = join " ", @line;
    print "Got request[$request]\n";
    my $erequest = URI::Escape::uri_escape($request, '^\w ');
    $erequest =~ s/ /+/g;
    my $url =
        sprintf("http://www.xray.mpe.mpg.de/cgi-bin/w3glimpse/modules?query=%s".
                "&errors=0&case=on&maxfiles=100&maxlines=30",
                $erequest,
               );
    print "$url\n\n";
    print ">>>>Trying to open a netscape window<<<<\n";
    system("netscape","-remote","openURL('$url')");
}

1;

__END__

=head1 NAME

 CPAN::Admin - A CPAN Shell for CPAN admins

=head1 SYNOPSIS

 perl -MCPAN::Admin -e shell

=head1 STATUS

Note: this module is currently not maintained. If you need it and fix
it for your needs, please submit patches.

=head1 DESCRIPTION

CPAN::Admin is a subclass of CPAN that adds the commands C<register>
and C<modsearch> to the CPAN shell.

C<register> calls C<get> on the named module, assembles a couple of
informations (description, language), and calls Netscape with the
-remote argument so that a form is filled with all the assembled
informations and the registration can be performed with a single
click. If the command line has more than one argument, register does
not run a C<get>, instead it interprets the rest of the line as DSLI
status, description, and userid and sends them to netscape such that
the form is again mostly filled and can be edited or confirmed with a
single click. CPAN::Admin never performs the submission click for you,
it is only intended to fill in the form on PAUSE and leave the
confirmation to you.

C<modsearch> simply passes the arguments to the search engine for the
modules@perl.org mailing list at http://www.xray.mpe.mpg.de where all
registration requests are stored. It does so in the same way as
register, namely with the C<netscape -remote> command.

An experimental feature has also been added, namely to color already
registered modules in listings. If you have Term::ANSIColor installed,
the u, r, and m commands will show already registered modules in
green.

=head1 PREREQISITES

URI::Escape, netscape browser available in the path, netscape must
understand the -remote switch (as far as I know, this is only
available on UNIX); coloring of registered modules is only available
if Term::ANSIColor is installed.

=head1 LICENSE

This program is free software; you can redistribute it and/or
modify it under the same terms as Perl itself.

=cut
