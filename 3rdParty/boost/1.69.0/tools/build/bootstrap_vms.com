$! Copyright 2015 Artur Shepilko.
$!
$! Distributed under the Boost Software License, Version 1.0.
$! (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
$!
$ THIS_FACILITY = "BOOSTBUILD"
$
$ verify = f$trnlnm("VERIFY_''THIS_FACILITY'")
$ save_verify = f$verify(verify)
$ save_default = f$env("DEFAULT")
$
$ SAY := WRITE SYS$OUTPUT
$
$ ON WARNING THEN CONTINUE
$ ON ERROR THEN GOTO ERROR
$
$ SAY "I|Bootstrapping the build engine..."
$
$ set def [.src.engine]
$ @build_vms /out=[--]bootstrap.log
$
$ set def 'save_default'
$
$ if f$search("[.src.engine.bin_vms]b2.exe") .eqs. "" then goto ERROR
$ copy [.src.engine.bin_vms]b2.exe []
$ copy [.src.engine.bin_vms]bjam.exe []
$
$ SAY "I|Bootstrapping is done, B2.EXE created."
$ type sys$input
$DECK

  To build and install under ROOT: directory, run:
    MC []B2 --prefix="/root" install

  Set B2 command:
    B2 :== $ROOT:[BIN]B2.EXE

$EOD
$ sts = 1
$
$EXIT:
$ set def 'save_default'
$ exit 'sts' + (0 * f$verify(save_verify))

$ERROR:
$ SAY "E|Failed to bootstrap build engine, see BOOTSTRAP.LOG for details."
$ sts = 4
$ goto EXIT
