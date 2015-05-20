package Bundle::LWP;

$VERSION = "5.810";

1;

__END__

=head1 NAME

Bundle::LWP - install all libwww-perl related modules

=head1 SYNOPSIS

 perl -MCPAN -e 'install Bundle::LWP'

=head1 CONTENTS

MIME::Base64       - Used in authentication headers

Digest::MD5        - Needed to do Digest authentication

URI 1.10           - There are URIs everywhere

Net::FTP 2.58      - If you want ftp://-support

HTML::Tagset       - Needed by HTML::Parser

HTML::Parser       - Need by HTML::HeadParser

HTML::HeadParser   - To get the correct $res->base

LWP                - The reason why you need the modules above

=head1 DESCRIPTION

This bundle defines all prereq modules for libwww-perl.  Bundles have
special meaning for the CPAN module.  When you install the bundle
module all modules mentioned in L</CONTENTS> will be installed instead.

=head1 SEE ALSO

L<CPAN/Bundles>
