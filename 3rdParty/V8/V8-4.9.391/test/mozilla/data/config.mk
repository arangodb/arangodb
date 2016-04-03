# -*- Mode: makefile -*-
# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla Communicator client code, released
# March 31, 1998.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK *****

# This file was adapted from mozilla/js/src/config.mk

# Set os+release dependent make variables
OS_ARCH         := $(subst /,_,$(shell uname -s | sed /\ /s//_/))

# Attempt to differentiate between SunOS 5.4 and x86 5.4
OS_CPUARCH      := $(shell uname -m)
ifeq ($(OS_CPUARCH),i86pc)
OS_RELEASE      := $(shell uname -r)_$(OS_CPUARCH)
else
ifeq ($(OS_ARCH),AIX)
OS_RELEASE      := $(shell uname -v).$(shell uname -r)
else
OS_RELEASE      := $(shell uname -r)
endif
endif
ifeq ($(OS_ARCH),IRIX64)
OS_ARCH         := IRIX
endif

# Handle output from win32 unames other than Netscape's version
ifeq (,$(filter-out Windows_95 Windows_98 CYGWIN_95-4.0 CYGWIN_98-4.10, $(OS_ARCH)))
	OS_ARCH   := WIN95
endif
ifeq ($(OS_ARCH),WIN95)
	OS_ARCH	   := WINNT
	OS_RELEASE := 4.0
endif
ifeq ($(OS_ARCH), Windows_NT)
	OS_ARCH    := WINNT
	OS_MINOR_RELEASE := $(shell uname -v)
	ifeq ($(OS_MINOR_RELEASE),00)
		OS_MINOR_RELEASE = 0
	endif
	OS_RELEASE := $(OS_RELEASE).$(OS_MINOR_RELEASE)
endif
ifeq (CYGWIN_NT,$(findstring CYGWIN_NT,$(OS_ARCH)))
	OS_RELEASE := $(patsubst CYGWIN_NT-%,%,$(OS_ARCH))
	OS_ARCH    := WINNT
endif
ifeq ($(OS_ARCH), CYGWIN32_NT)
	OS_ARCH    := WINNT
endif

# Virtually all Linux versions are identical.
# Any distinctions are handled in linux.h
ifeq ($(OS_ARCH),Linux)
OS_CONFIG      := Linux_All
else
ifeq ($(OS_ARCH),dgux)
OS_CONFIG      := dgux
else
ifeq ($(OS_ARCH),Darwin)
OS_CONFIG      := Darwin
else
OS_CONFIG       := $(OS_ARCH)$(OS_OBJTYPE)$(OS_RELEASE)
endif
endif
endif

ifeq "$(TEST_OPTDEBUG)" "opt"
OBJDIR_TAG = _OPT
else
OBJDIR_TAG = _DBG
endif


# Name of the binary code directories
ifdef BUILD_IDG
JS_OBJDIR          = $(OS_CONFIG)$(OBJDIR_TAG).OBJD
else
JS_OBJDIR          = $(OS_CONFIG)$(OBJDIR_TAG).OBJ
endif

