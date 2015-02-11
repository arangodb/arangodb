;#
#
# This library is no longer being maintained, and is included for backward
# compatibility with Perl 4 programs which may require it.
#
# In particular, this should not be used as an example of modern Perl
# programming techniques.
#
# Suggested alternative: Term::Complete
#
;#      @(#)complete.pl,v1.1            (me@anywhere.EBay.Sun.COM) 09/23/91
;#
;# Author: Wayne Thompson
;#
;# Description:
;#     This routine provides word completion.
;#     (TAB) attempts word completion.
;#     (^D)  prints completion list.
;#      (These may be changed by setting $Complete'complete, etc.)
;#
;# Diagnostics:
;#     Bell when word completion fails.
;#
;# Dependencies:
;#     The tty driver is put into raw mode.
;#
;# Bugs:
;#
;# Usage:
;#     $input = &Complete('prompt_string', *completion_list);
;#         or
;#     $input = &Complete('prompt_string', @completion_list);
;#

CONFIG: {
    package Complete;

    $complete = "\004";
    $kill     = "\025";
    $erase1 =   "\177";
    $erase2 =   "\010";
}

sub Complete {
    package Complete;

    local($prompt, @cmp_list, $return, @match, $l, $test, $cmp, $r);
    if ($_[1] =~ /^StB\0/) {
        ($prompt, *_) = @_;
    }
    else {
        $prompt = shift(@_);
    }
    @cmp_lst = sort(@_);

    system('stty raw -echo');
    LOOP: {
        print($prompt, $return);
        while (($_ = getc(STDIN)) ne "\r") {
            CASE: {
                # (TAB) attempt completion
                $_ eq "\t" && do {
                    @match = grep(/^$return/, @cmp_lst);
                    $l = length($test = shift(@match));
                    unless ($#match < 0) {
                        foreach $cmp (@match) {
                            until (substr($cmp, 0, $l) eq substr($test, 0, $l)) {
                                $l--;
                            }
                        }
                        print("\a");
                    }
                    print($test = substr($test, $r, $l - $r));
                    $r = length($return .= $test);
                    last CASE;
                };

                # (^D) completion list
                $_ eq $complete && do {
                    print(join("\r\n", '', grep(/^$return/, @cmp_lst)), "\r\n");
                    redo LOOP;
                };

                # (^U) kill
                $_ eq $kill && do {
                    if ($r) {
                        undef $r;
			undef $return;
                        print("\r\n");
                        redo LOOP;
                    }
                    last CASE;
                };

                # (DEL) || (BS) erase
                ($_ eq $erase1 || $_ eq $erase2) && do {
                    if($r) {
                        print("\b \b");
                        chop($return);
                        $r--;
                    }
                    last CASE;
                };

                # printable char
                ord >= 32 && do {
                    $return .= $_;
                    $r++;
                    print;
                    last CASE;
                };
            }
        }
    }
    system('stty -raw echo');
    print("\n");
    $return;
}

1;
