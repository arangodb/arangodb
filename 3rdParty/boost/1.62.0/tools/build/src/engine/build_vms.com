$ ! Copyright 2002-2003 Rene Rivera, Johan Nilsson.
$ !
$ ! 8-APR-2004 Boris Gubenko
$ ! Miscellaneous improvements.
$ !
$ ! 20-JAN-2015 Artur Shepilko
$ ! Adapt for jam 3.1.19
$ !
$ ! Distributed under the Boost Software License, Version 1.0.
$ ! (See accompanying file LICENSE_1_0.txt or http://www.boost.org/LICENSE_1_0.txt)
$ !
$ ! bootstrap build script for Jam
$ !
$ THIS_FACILITY = "BUILDJAM"
$
$ verify = f$trnlnm("VERIFY_''THIS_FACILITY'")
$ save_verify = f$verify(verify)
$
$ SAY := WRITE SYS$OUTPUT
$ !
$ ON WARNING THEN CONTINUE
$ ON ERROR THEN GOTO EXIT
$
$ BOOST_JAM_TOOLSET = "vmsdecc"
$ BOOST_JAM_CC = "CC"
$ BJAM_UPDATE = ""
$
$ ARGS = F$EDIT("''p1' ''p2' ''p3' ''p4'","TRIM,LOWERCASE")
$ ARGS_LEN = F$LENGTH(ARGS)
$
$ IF F$LOCATE("--update", ARGS) .NE. F$LENGTH(ARGS) THEN BJAM_UPDATE = "update"
$ IF BJAM_UPDATE .EQS. "update" -
    .AND. F$SEARCH("[.bootstrap_vms]jam0.exe") .EQS. "" THEN BJAM_UPDATE = ""
$
$ IF BJAM_UPDATE .NES. "update"
$ THEN
$   GOSUB CLEAN
$
$   SAY "I|Creating bootstrap directory..."
$   CREATE /DIR [.bootstrap_vms]
$
$   !------------------
$   ! NOTE: Assume jamgram and jambase have been generated (true for fresh release).
$   ! Otherwise these need to be re-generated manually.
$   !------------------
$
$   SAY "I|Building bootstrap jam..."
$   !
$   CC_FLAGS = "/DEFINE=VMS /STANDARD=VAXC " + -
      "/PREFIX_LIBRARY_ENTRIES=(ALL_ENTRIES) " + -
      "/WARNING=DISABLE=(LONGEXTERN)" + -
      "/OBJ=[.bootstrap_vms] "
$
$   CC_INCLUDE=""
$
$   SAY "I|Using compile flags: ", CC_FLAGS
$
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE command.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE compile.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE constants.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE debug.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE execcmd.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE frames.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE function.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE glob.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE hash.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE hdrmacro.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE headers.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE jam.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE jambase.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE jamgram.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE lists.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE make.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE make1.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE object.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE option.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE output.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE parse.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE pathsys.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE regexp.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE rules.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE scan.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE search.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE subst.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE timestamp.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE variable.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE modules.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE strings.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE filesys.c
$
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE execvms.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE pathvms.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE filevms.c
$
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE builtins.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE class.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE cwd.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE native.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE md5.c
$
$   CC_INCLUDE = "/INCLUDE=(""./modules"")"
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]set.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]path.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]regex.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]property-set.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]sequence.c
$   'BOOST_JAM_CC' 'CC_FLAGS 'CC_INCLUDE [.modules]order.c
$
$   LIB /CREATE [.bootstrap_vms]jam0.olb [.bootstrap_vms]*.obj
$   LINK /EXEC=[.bootstrap_vms]jam0.exe -
      [.bootstrap_vms]jam0.olb/INCLUDE=JAM/LIB
$
$   IF F$SEARCH("[.bootstrap_vms]*.obj") .NES. "" THEN -
      DELETE /NOCONF /NOLOG [.bootstrap_vms]*.obj;*, *.olb;*
$ ENDIF
$
$ IF F$SEARCH("[.bootstrap_vms]jam0.exe") .NES. ""
$ THEN
$   IF BJAM_UPDATE .NES. "update"
$   THEN
$     SAY "I|Cleaning previous build..."
$     MCR [.bootstrap_vms]jam0.exe -f build.jam --toolset='BOOST_JAM_TOOLSET' 'ARGS' clean
$   ENDIF
$
$   SAY "I|Building Boost.Jam..."
$   MCR [.bootstrap_vms]jam0.exe -f build.jam --toolset='BOOST_JAM_TOOLSET' 'ARGS'
$ ENDIF
$
$
$EXIT:
$ sts = $STATUS
$ exit 'sts' + (0 * f$verify(save_verify))


$CLEAN: !GOSUB
$ !
$ IF F$SEARCH("[.bootstrap_vms]*.*") .NES. ""
$ THEN
$   SAY "I|Cleaning previous bootstrap files..."
$ !
$   SET FILE /PROT=(W:RWED) [.bootstrap_vms]*.*;*
$   DELETE /NOCONF /NOLOG [.bootstrap_vms]*.*;*
$ ENDIF
$ !
$ IF F$SEARCH("bootstrap_vms.dir") .NES. ""
$ THEN
$   SAY "I|Removing previous bootstrap directory..."
$ !
$   SET FILE /PROT=(W:RWED) bootstrap_vms.dir
$   DELETE /NOCONF /NOLOG bootstrap_vms.dir;
$ ENDIF
$ !
$ RETURN
