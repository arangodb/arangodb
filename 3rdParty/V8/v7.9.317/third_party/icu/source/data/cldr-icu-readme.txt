# Copyright (C) 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html
# Copyright (C) 2010-2014, International Business Machines Corporation and others.
# All Rights Reserved.                  
#
# Commands for regenerating ICU4C locale data (.txt files) from CLDR.
#
# The process requires local copies of
#    - CLDR (the source of most of the data, and some Java tools)
#    - ICU4J  (used only for checking the converted data)
#    - ICU4C  (the destination for the new data, and the source for some of it)
#
# For an official CLDR data integration into ICU, these should be clean, freshly
# checked-out. For released CLDR sources, an alternative to checking out sources
# for a given version is downloading the zipped sources for the common (core.zip)
# and tools (tools.zip) directory subtrees from the Data column in
# [http://cldr.unicode.org/index/downloads].
#
# The versions of each of these must match. Included with the release notes for
# ICU is the version number and/or a CLDR svn tag name for the revision of CLDR
# that was the source of the data for that release of ICU.
#
# Besides a standard JDK, the process also requires ant
# (http://ant.apache.org/),
# plus the xml-apis.jar from the Apache xalan package
# (http://xml.apache.org/xalan-j/downloads.html).
#
# Note: Enough things can (and will) fail in this process that it is best to
#   run the commands separately from an interactive shell. They should all
#   copy and paste without problems.
#
# It is often useful to save logs of the output of many of the steps in this
# process. The commands below put log files in /tmp; you may want to put them
# somewhere else.
#
#----
#
# CLDR dtd caching
#
# Parts of the build process (notably building the new ICU data files in step 4)
# require http: access to files in the CLDR repository; for example, processing
# the files in icu4c/source/data/xml/ may require access to
# http://www.unicode.org/repos/cldr/trunk/common/dtd/ldml.dtd
#
# Unless you cache the dtds locally by
# 1. creating a temp directory e.g. ~/.cldrdtd
# 2. setting CLDR_DTD_CACHE to it e.g. -DCLDR_DTD_CACHE=~/.cldrdtd
# your system will make excessive network requests, which will result in the
# Unicode server blocking access.
#
#----
#
# There are several environment variables that need to be defined.
#
# a) Java- and ant-related variables
#
# JAVA_HOME:     Path to JDK (a directory, containing e.g. bin/java, bin/javac,
#                etc.); on many systems this can be set using
#                `/usr/libexec/java_home`.
#
# ANT_OPTS:      You may want to set:
#
#                -Xmx4096m, to give Java more memory; otherwise it may run out
#                 of heap.
#                -DCLDR_DTD_CACHE=~/.cldrdtd (or some other temp directory
#                 that already exists), to reduce frequent http: access to dtds
#                 and consequent blockage by Unicode server.
#
# b) CLDR-related variables
#
# CLDR_DIR:      Path to root of CLDR sources, below which are the common and
#                tools directories.
# CLDR_CLASSES:  Path to the CLDR Tools classes directory. If not set, defaults
#                to $CLDR_DIR/tools/java/classes
#
# c) ICU-related variables
# These variables only need to be set if you're directly reusing the
# commands below.
#
# ICU4C_DIR:     Path to root of ICU4C sources, below which is the source dir.
#
# ICU4J_ROOT:    Path to root of ICU4J sources, below which is the main dir.
#
#----
#
# If you are adding or removing locales, or specific kinds of locale data,
# there are some xml files in the ICU sources that need to be updated (these xml
# files are used in addition to the CLDR files as inputs to the CLDR data build
# process for ICU):
#
#    icu4c/source/data/icu-config.xml - Update <locales> to add or remove
#                CLDR locales for inclusion in ICU. Update <paths> to prefer
#                alt forms for certain paths, or to exclude certain paths; note
#                that <paths> items can only have draft or alt attributes.
#
#                Note that if a language-only locale (e.g. "de") is included in
#                <locales>, then all region sublocales for that language that
#                are present in CLDR data (e.g. "de_AT", "de_BE", "de_CH", etc.)
#                should also be included in <locales>, per PMC policy decision
#                2012-05-02 (see http://bugs.icu-project.org/trac/ticket/9298).
#
#    icu4c/source/data/build.xml - If you are adding or removing break
#                iterators, you need to update  <fileset id="brkitr" ...> under
#                <target name="clean" ...> to clean the correct set of files.
#
#                If there are new CLDR resource bundle types, you may need to
#                updated the <remapper> sections to put these in the correct
#                data subfolder for ICU.
#
#    icu4c/source/data/xml/      - If you are adding a new locale, break
#                iterator, collation tailoring, or rule-based number formatter,
#                you may need to add a corresponding xml file in (respectively)
#                the main/, brkitr/, collation/, or rbnf/ subdirectory here.
#
#----
#
# For an official CLDR data integration into ICU, there are some additional
# considerations:
#
# a) Don't commit anything in ICU sources (and possibly any changes in CLDR
#    sources, depending on their nature) until you have finished testing and
#    resolving build issues and test failures for both ICU4C and ICU4J.
#
# b) There are version numbers that may need manual updating in CLDR (other
#    version numbers get updated automatically, based on these):
#
#    common/dtd/ldml.dtd                            - update cldrVersion
#    common/dtd/ldmlBCP47.dtd                       - update cldrVersion
#    common/dtd/ldmlSupplemental.dtd                - update cldrVersion
#    common/dtd/ldmlSupplemental.dtd                - updateunicodeVersion
#    keyboards/dtd/ldmlKeyboard.dtd                 - update cldrVersion
#    tools/java/org/unicode/cldr/util/CLDRFile.java - update GEN_VERSION
#
# c) After everything is committed, you will need to tag the CLDR, ICU4J, and
#    ICU4C sources that ended up being used for the integration; see step 17
#    below.
#
################################################################################

# 1a. Java and ant variables, adjust for your system

export JAVA_HOME=`/usr/libexec/java_home`
export ANT_OPTS="-Xmx4096m -DCLDR_DTD_CACHE=~/.cldrdtd"

# 1b. CLDR variables, adjust for your setup; with cygwin it might be e.g.
# CLDR_DIR=`cygpath -wp /build/cldr`

export CLDR_DIR=$HOME/cldr/trunk

# 1c. ICU variables

export ICU4C_DIR=$HOME/icu/icu4c
export ICU4J_ROOT=$HOME/icu/icu4j

# 1d. Pre-populate your CLDR DTD cache. You need to do this only once.

mkdir ~/.cldrdtd
cd ~/.cldrdtd
curl http://www.unicode.org/repos/cldr/trunk/common/dtd/ldml.dtd \
    -o http___www.unicode.org_repos_cldr_trunk_common_dtd_ldml.dtd
# WAIT before hitting the server again; it WILL NOT give you a second chance!
sleep 5
curl http://www.unicode.org/repos/cldr/trunk/common/dtd/ldmlICU.dtd \
    -o http___www.unicode.org_repos_cldr_trunk_common_dtd_ldmlICU.dtd
sleep 5
curl http://www.unicode.org/repos/cldr/trunk/common/dtd/ldmlSupplemental.dtd \
    -o http___www.unicode.org_repos_cldr_trunk_common_dtd_ldmlSupplemental.dtd

# 2. Build the CLDR Java tools
# Optionally build the jar, but ant will look inside the classes directory anyway

cd $CLDR_DIR/tools/java
ant all
#ant jar

# 3. Configure ICU4C, build and test without new data first, to verify that
# there are no pre-existing errors. Here <platform> is the runConfigureICU
# code for the platform you are building, e.g. Linux, MacOSX, Cygwin.

cd $ICU4C_DIR/source
./runConfigureICU <platform>
make all 2>&1 | tee /tmp/icu4c-oldData-makeAll.txt
make check 2>&1 | tee /tmp/icu4c-oldData-makeCheck.txt

# 4. Build the new ICU4C data files; these include .txt files and .mk files.
# These new files will replace whatever was already present in the ICU4C sources.
# This process uses ant with ICU's data/build.xml and data/icu-config.xml to
# operate (via CLDR's ant/CLDRConverterTool.java and ant/CLDRBuild.java) the
# necessary CLDR tools including LDML2ICUConverter, ConvertTransforms, etc.
# This process will take several minutes.
# Keep a log so you can investigate anything that looks suspicious.
#
# Running "ant setup" is not required, but it will print useful errors to
# debug issues with your path when it fails.
#
# If you see timeout errors when building the rbnf data, for example, then you are
# likely not using a CLDR dtd cache; see "CLDR dtd caching" above. If you are using
# a dtd cache and are still having timeout problems, the IP address of your system
# may have been blocked due to previous excessive access. In this case you may need
# to contact a Unicode sysadmin to restore access.
#
# Unfortunately, even if you have your DTD cache variable enabled, you may still
# get blocked and unable to populate your cache because of multiple successive
# requests to download the required DTD files. It is recommended that you
# pre-populate your cache as shown above in step 1d.

cd $ICU4C_DIR/source/data
ant setup
ant clean
ant all 2>&1 | tee /tmp/cldr-newData-buildLog.txt

# 5. Check which data files have modifications, which have been added or removed
# (if there are no changes, you may not need to proceed further). Make sure the
# list seems reasonable.

git status

# 6. Fix any errors, investigate any warnings. Currently for example there are
# a few warnings of the following form in rbnf files:
#   [cldr-build] Warning: no version match with: $Revision☹$
#
# Fixing may entail modifying CLDR source data or tools - for example,
# updating the validSubLocales for collation data (file a bug if appropriate).
# Repeat steps 4-5 until there are no build errors and no unexpected
# warnings.

# 7. Now rebuild ICU4C with the new data and run make check tests.
# Again, keep a log so you can investigate the errors.
cd $ICU4C_DIR/source

# 7a. If any files were added or removed (likely), re-run configure:
./runConfigureICU <platform>
make clean

# 7b. Now do the rebuild.
make check 2>&1 | tee /tmp/icu4c-newData-makeCheck.txt

# 8. Investigate each test case failure. The first run processing new CLDR data
# from the Survey Tool can result in thousands of failures (in many cases, one
# CLDR data fix can resolve hundreds of test failures). If the error is caused
# by bad CLDR data, then file a CLDR bug, fix the data, and regenerate from
# step 4. If the data is OK but the testcase needs to be updated because the
# data has legitimately changed, then update the testcase. You will check in
# the updated testcases along with the new ICU data at the end of this process.
# Note that if the new data has any differences in structure, you will have to
# update test/testdata/structLocale.txt or /tsutil/cldrtest/TestLocaleStructure
# may fail.
# Repeat steps 4-7 until there are no errors.

# 9. Now run the make check tests in exhaustive mode:

cd $ICU4C_DIR/source
export INTLTEST_OPTS="-e"
export CINTLTST_OPTS="-e"
make check 2>&1 | tee /tmp/icu4c-newData-makeCheckEx.txt

# 10. Again, investigate each failure, fixing CLDR data or ICU test cases as
# appropriate, and repeating steps 4-7 and 9 until there are no errors.

# 11. Now with ICU4J, build and test without new data first, to verify that
# there are no pre-existing errors (or at least to have the pre-existing errors
# as a base for comparison):

cd $ICU4J_ROOT
ant all 2>&1 | tee /tmp/icu4j-oldData-antAll.txt
ant check 2>&1 | tee /tmp/icu4j-oldData-antCheck.txt

# 12. Transfer the data to ICU4J:
cd $ICU4C_DIR/source

# 12a. You need to reconfigure ICU4C to include the unicore data.
ICU_DATA_BUILDTOOL_OPTS=--include_uni_core_data ./runConfigureICU <platform>

# 12b. Now build the jar files.
cd $ICU4C_DIR/source/data
# The following 2 lines are required to include the unicore data:
  make clean
  make -j6
make icu4j-data-install
cd $ICU4C_DIR/source/test/testdata
make icu4j-data-install

# 13. Now rebuild ICU4J with the new data and run tests:
# Keep a log so you can investigate the errors.

cd $ICU4J_ROOT
ant check 2>&1 | tee /tmp/icu4j-newData-antCheck.txt

# 14. Investigate test case failures; fix test cases and repeat from step 12,
# or fix CLDR data and repeat from step 4, as appropriate, until there are no
# more failures in ICU4C or ICU4J (except failures that were present before you
# began testing the new CLDR data).

# 15. Check the file changes; then git add or git rm as necessary, and
# commit the changes.

cd $HOME/icu/
cd ..
git status
# add or remove as necessary
# commit

# 16. For an official CLDR data integration into ICU, now tag the CLDR and
# ICU sources with an appropriate CLDR milestone (you can check previous
# tags for format), e.g.:

svn copy svn+ssh://unicode.org/repos/cldr/trunk \
svn+ssh://unicode.org/repos/cldr/tags/release-NNN \
--parents -m "cldrbug nnnn: tag cldr sources for NNN"

cd $HOME/icu/
git tag ...


