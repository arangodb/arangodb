# LWPExternEnt.pl
#
# Copyright (c) 2000 Clark Cooper
# All rights reserved.
#
# This program is free software; you can redistribute it and/or
# modify it under the same terms as Perl itself.

package XML::Parser;

use URI;
use URI::file;
use LWP;

##
## Note that this external entity handler reads the entire entity into
## memory, so it will choke on huge ones. It would be really nice if
## LWP::UserAgent optionally returned us an IO::Handle.
##

sub lwp_ext_ent_handler {
  my ($xp, $base, $sys) = @_;  # We don't use public id

  my $uri;

  if (defined $base) {
    # Base may have been set by parsefile, which is agnostic about
    # whether its a file or URI.
    my $base_uri = new URI($base);
    unless (defined $base_uri->scheme) {
      $base_uri = URI->new_abs($base_uri, URI::file->cwd);
    }

    $uri = URI->new_abs($sys, $base_uri);
  }
  else {
    $uri = new URI($sys);
    unless (defined $uri->scheme) {
      $uri = URI->new_abs($uri, URI::file->cwd);
    }
  }
  
  my $ua = $xp->{_lwpagent};
  unless (defined $ua) {
    $ua = $xp->{_lwpagent} = new LWP::UserAgent();
    $ua->env_proxy();
  }

  my $req = new HTTP::Request('GET', $uri);

  my $res = $ua->request($req);
  if ($res->is_error) {
    $xp->{ErrorMessage} .= "\n" . $res->status_line . " $uri";
    return undef;
  }
  
  $xp->{_BaseStack} ||= [];
  push(@{$xp->{_BaseStack}}, $base);

  $xp->base($uri);
  
  return $res->content;
}  # End lwp_ext_ent_handler

sub lwp_ext_ent_cleanup {
  my ($xp) = @_;

  $xp->base(pop(@{$xp->{_BaseStack}}));
}  # End lwp_ext_ent_cleanup

1;
