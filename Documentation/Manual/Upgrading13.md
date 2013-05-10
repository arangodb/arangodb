Upgrading to ArangoDB 1.3 {#Upgrading13}
========================================

@NAVIGATE_Upgrading13
@EMBEDTOC{Upgrading13TOC}

Upgrading {#Upgrading13Introduction}
====================================

ArangoDB 1.3 provides a lot of new features and APIs when compared to ArangoDB
1.2. The most important one being true multi-collection transactions support.

The following list contains changes in ArangoDB 1.3 that are not 100%
downwards-compatible to ArangoDB 1.2.

Existing users of ArangoDB 1.2 should read the list carefully and make sure they
have undertaken all necessary steps and precautions before upgrading from
ArangoDB 1.2 to ArangoDB 1.3. Please also check @ref Upgrading13Troubleshooting.

New and Changed Command-Line Options{#Upgrading13Options}
---------------------------------------------------------

In order to support node modules and packages, a new command-line options was
introduced:

    --javascript.package-path <directory>

must be used to specify the directory containing the NPM packages. This is option
is presented in the pre-defined configuration files. In case a created your own
configuration, you need to add this option and make sure that

    --javascript.modules-path <directories>

also contains the new node directory.


Troubleshooting{#Upgrading13Troubleshooting}
============================================

