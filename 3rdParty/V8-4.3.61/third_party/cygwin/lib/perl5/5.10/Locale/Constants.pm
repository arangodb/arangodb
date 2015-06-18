#
# Locale::Constants - defined constants for identifying codesets
#
# $Id: Constants.pm,v 2.7 2004/06/10 21:19:34 neilb Exp $
#

package Locale::Constants;
use strict;

require Exporter;

#-----------------------------------------------------------------------
#	Public Global Variables
#-----------------------------------------------------------------------
use vars qw($VERSION @ISA @EXPORT);
$VERSION   = sprintf("%d.%02d", q$Revision: 2.7 $ =~ /(\d+)\.(\d+)/);
@ISA	= qw(Exporter);
@EXPORT = qw(LOCALE_CODE_ALPHA_2 LOCALE_CODE_ALPHA_3 LOCALE_CODE_NUMERIC
		LOCALE_CODE_DEFAULT);

#-----------------------------------------------------------------------
#	Constants
#-----------------------------------------------------------------------
use constant LOCALE_CODE_ALPHA_2 => 1;
use constant LOCALE_CODE_ALPHA_3 => 2;
use constant LOCALE_CODE_NUMERIC => 3;

use constant LOCALE_CODE_DEFAULT => LOCALE_CODE_ALPHA_2;

1;

