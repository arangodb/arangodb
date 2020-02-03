# © 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html#License
# A list of txt's to build
#
#  * To add an additional locale to the list:
#    _____________________________________________________
#    |  SPREP_SOURCE_LOCAL =   myStringPrep.txt ...
#
#  * To REPLACE the default list and only build a subset of files:
#    _____________________________________________________
#    |  SPREP_SOURCE = rfc4518.txt
#
# Chromium: Drop all the stringprep files including RFC 3491 for IDNA 2003.
# ICU supports UTS 46 (IDNA2003, transition, and IDNA 2008), but Chromium
# does not use IDNA 2003 any more and uses transitional/IDNA 2008 for which
# a separate resource (uts46.nrm) is used. Therefore, we can drop rfc3491 as
# well.
# If we ever need to support other stringprep (e.g. LDAP, XMPP, SASL),
# we have to add back the corresponding prep files.
# rfc3491 : stringprep profile for IDN 2003
# rfc3530 : NFS
# rfc3722 : String Profile for Internet Small Computer
#           Systems Interface (iSCSI) Names
# rfc3920 : XMPP (Extensible Messaging and Presence Protocol)
# rfc4011 : Policy Based Management Information Base, SNMP
# rfc4013 : SASLprep: Stringprep Profile for User Names and Passwords
# rfc4505 :  Anonymous Simple Authentication and Security Layer (SASL) Mechanism
# rfc4518 : LDAP  Internationalized String Preparation
#
SPREP_SOURCE =
