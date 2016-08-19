#.rst:
# GNUInstallDirs
# --------------
#
# Define GNU standard installation directories
#
# Provides install directory variables as defined by the
# `GNU Coding Standards`_.
#
# .. _`GNU Coding Standards`: https://www.gnu.org/prep/standards/html_node/Directory-Variables.html
#
# Result Variables
# ^^^^^^^^^^^^^^^^
#
# Inclusion of this module defines the following variables:
#
# ``CMAKE_INSTALL_<dir>``
#
#   Destination for files of a given type.  This value may be passed to
#   the ``DESTINATION`` options of :command:`install` commands for the
#   corresponding file type.
#
# ``CMAKE_INSTALL_FULL_<dir>``
#
#   The absolute path generated from the corresponding ``CMAKE_INSTALL_<dir>``
#   value.  If the value is not already an absolute path, an absolute path
#   is constructed typically by prepending the value of the
#   :variable:`CMAKE_INSTALL_PREFIX` variable.  However, there are some
#   `special cases`_ as documented below.
#
# where ``<dir>`` is one of:
#
# ``BINDIR``
#   user executables (``bin``)
# ``SBINDIR``
#   system admin executables (``sbin``)
# ``LIBEXECDIR``
#   program executables (``libexec``)
# ``SYSCONFDIR``
#   read-only single-machine data (``etc``)
# ``SHAREDSTATEDIR``
#   modifiable architecture-independent data (``com``)
# ``LOCALSTATEDIR``
#   modifiable single-machine data (``var``)
# ``LIBDIR``
#   object code libraries (``lib`` or ``lib64``
#   or ``lib/<multiarch-tuple>`` on Debian)
# ``INCLUDEDIR``
#   C header files (``include``)
# ``OLDINCLUDEDIR``
#   C header files for non-gcc (``/usr/include``)
# ``DATAROOTDIR``
#   read-only architecture-independent data root (``share``)
# ``DATADIR``
#   read-only architecture-independent data (``DATAROOTDIR``)
# ``INFODIR``
#   info documentation (``DATAROOTDIR/info``)
# ``LOCALEDIR``
#   locale-dependent data (``DATAROOTDIR/locale``)
# ``MANDIR``
#   man documentation (``DATAROOTDIR/man``)
# ``DOCDIR``
#   documentation root (``DATAROOTDIR/doc/PROJECT_NAME``)
#
# If the includer does not define a value the above-shown default will be
# used and the value will appear in the cache for editing by the user.
#
# Special Cases
# ^^^^^^^^^^^^^
#
# The following values of :variable:`CMAKE_INSTALL_PREFIX` are special:
#
# ``/``
#
#   For ``<dir>`` other than the ``SYSCONFDIR`` and ``LOCALSTATEDIR``,
#   the value of ``CMAKE_INSTALL_<dir>`` is prefixed with ``usr/`` if
#   it is not user-specified as an absolute path.  For example, the
#   ``INCLUDEDIR`` value ``include`` becomes ``usr/include``.
#   This is required by the `GNU Coding Standards`_, which state:
#
#     When building the complete GNU system, the prefix will be empty
#     and ``/usr`` will be a symbolic link to ``/``.
#
# ``/usr``
#
#   For ``<dir>`` equal to ``SYSCONFDIR`` or ``LOCALSTATEDIR``, the
#   ``CMAKE_INSTALL_FULL_<dir>`` is computed by prepending just ``/``
#   to the value of ``CMAKE_INSTALL_<dir>`` if it is not user-specified
#   as an absolute path.  For example, the ``SYSCONFDIR`` value ``etc``
#   becomes ``/etc``.  This is required by the `GNU Coding Standards`_.
#
# ``/opt/...``
#
#   For ``<dir>`` equal to ``SYSCONFDIR`` or ``LOCALSTATEDIR``, the
#   ``CMAKE_INSTALL_FULL_<dir>`` is computed by *appending* the prefix
#   to the value of ``CMAKE_INSTALL_<dir>`` if it is not user-specified
#   as an absolute path.  For example, the ``SYSCONFDIR`` value ``etc``
#   becomes ``/etc/opt/...``.  This is defined by the
#   `Filesystem Hierarchy Standard`_.
#
# .. _`Filesystem Hierarchy Standard`: https://refspecs.linuxfoundation.org/FHS_3.0/fhs/index.html

#=============================================================================
# Copyright 2015 Alex Turbov <i.zaufi@gmail.com>
# Copyright 2011 Nikita Krupen'ko <krnekit@gmail.com>
# Copyright 2011 Kitware, Inc.
#
# Distributed under the OSI-approved BSD License (the "License");
# see accompanying file Copyright.txt for details.
#
# This software is distributed WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the License for more information.
#=============================================================================
# (To distribute this file outside of CMake, substitute the full
#  License text for the above reference.)

# Installation directories
#
if(NOT DEFINED CMAKE_INSTALL_BINDIR)
  set(CMAKE_INSTALL_BINDIR "bin" CACHE PATH "user executables (bin)")
endif()

if(NOT DEFINED CMAKE_INSTALL_SBINDIR)
  set(CMAKE_INSTALL_SBINDIR "sbin" CACHE PATH "system admin executables (sbin)")
endif()

if(NOT DEFINED CMAKE_INSTALL_LIBEXECDIR)
  set(CMAKE_INSTALL_LIBEXECDIR "libexec" CACHE PATH "program executables (libexec)")
endif()

if(NOT DEFINED CMAKE_INSTALL_SYSCONFDIR)
  set(CMAKE_INSTALL_SYSCONFDIR "etc" CACHE PATH "read-only single-machine data (etc)")
endif()

if(NOT DEFINED CMAKE_INSTALL_SHAREDSTATEDIR)
  set(CMAKE_INSTALL_SHAREDSTATEDIR "com" CACHE PATH "modifiable architecture-independent data (com)")
endif()

if(NOT DEFINED CMAKE_INSTALL_LOCALSTATEDIR)
  set(CMAKE_INSTALL_LOCALSTATEDIR "var" CACHE PATH "modifiable single-machine data (var)")
endif()

# We check if the variable was manually set and not cached, in order to
# allow projects to set the values as normal variables before including
# GNUInstallDirs to avoid having the entries cached or user-editable. It
# replaces the "if(NOT DEFINED CMAKE_INSTALL_XXX)" checks in all the
# other cases.
# If CMAKE_INSTALL_LIBDIR is defined, if _libdir_set is false, then the
# variable is a normal one, otherwise it is a cache one.
get_property(_libdir_set CACHE CMAKE_INSTALL_LIBDIR PROPERTY TYPE SET)
if(NOT DEFINED CMAKE_INSTALL_LIBDIR OR (_libdir_set
    AND DEFINED _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX
    AND NOT "${_GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX}" STREQUAL "${CMAKE_INSTALL_PREFIX}"))
  # If CMAKE_INSTALL_LIBDIR is not defined, it is always executed.
  # Otherwise:
  #  * if _libdir_set is false it is not executed (meaning that it is
  #    not a cache variable)
  #  * if _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX is not defined it is
  #    not executed
  #  * if _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX and
  #    CMAKE_INSTALL_PREFIX are the same string it is not executed.
  #    _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX is updated after the
  #    execution, of this part of code, therefore at the next inclusion
  #    of the file, CMAKE_INSTALL_LIBDIR is defined, and the 2 strings
  #    are equal, meaning that the if is not executed the code the
  #    second time.

  set(_LIBDIR_DEFAULT "lib")
  # Override this default 'lib' with 'lib64' iff:
  #  - we are on Linux system but NOT cross-compiling
  #  - we are NOT on debian
  #  - we are on a 64 bits system
  # reason is: amd64 ABI: http://www.x86-64.org/documentation/abi.pdf
  # For Debian with multiarch, use 'lib/${CMAKE_LIBRARY_ARCHITECTURE}' if
  # CMAKE_LIBRARY_ARCHITECTURE is set (which contains e.g. "i386-linux-gnu"
  # and CMAKE_INSTALL_PREFIX is "/usr"
  # See http://wiki.debian.org/Multiarch
  if(DEFINED _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
    set(__LAST_LIBDIR_DEFAULT "lib")
    # __LAST_LIBDIR_DEFAULT is the default value that we compute from
    # _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX, not a cache entry for
    # the value that was last used as the default.
    # This value is used to figure out whether the user changed the
    # CMAKE_INSTALL_LIBDIR value manually, or if the value was the
    # default one. When CMAKE_INSTALL_PREFIX changes, the value is
    # updated to the new default, unless the user explicitly changed it.
  endif()
  if(CMAKE_SYSTEM_NAME MATCHES "^(Linux|kFreeBSD|GNU)$"
      AND NOT CMAKE_CROSSCOMPILING)
    if (EXISTS "/etc/debian_version") # is this a debian system ?
      if(CMAKE_LIBRARY_ARCHITECTURE)
        if("${CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
          set(_LIBDIR_DEFAULT "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        if(DEFINED _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX
            AND "${_GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
          set(__LAST_LIBDIR_DEFAULT "lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
      endif()
    else() # not debian, rely on CMAKE_SIZEOF_VOID_P:
      if(NOT DEFINED CMAKE_SIZEOF_VOID_P)
        message(AUTHOR_WARNING
          "Unable to determine default CMAKE_INSTALL_LIBDIR directory because no target architecture is known. "
          "Please enable at least one language before including GNUInstallDirs.")
      else()
        if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
          set(_LIBDIR_DEFAULT "lib64")
          if(DEFINED _GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX)
            set(__LAST_LIBDIR_DEFAULT "lib64")
          endif()
        endif()
      endif()
    endif()
  endif()
  if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR "${_LIBDIR_DEFAULT}" CACHE PATH "object code libraries (${_LIBDIR_DEFAULT})")
  elseif(DEFINED __LAST_LIBDIR_DEFAULT
      AND "${__LAST_LIBDIR_DEFAULT}" STREQUAL "${CMAKE_INSTALL_LIBDIR}")
    set_property(CACHE CMAKE_INSTALL_LIBDIR PROPERTY VALUE "${_LIBDIR_DEFAULT}")
  endif()
endif()
# Save for next run
set(_GNUInstallDirs_LAST_CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" CACHE INTERNAL "CMAKE_INSTALL_PREFIX during last run")
unset(_libdir_set)
unset(__LAST_LIBDIR_DEFAULT)


if(NOT DEFINED CMAKE_INSTALL_INCLUDEDIR)
  set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE PATH "C header files (include)")
endif()

if(NOT DEFINED CMAKE_INSTALL_OLDINCLUDEDIR)
  set(CMAKE_INSTALL_OLDINCLUDEDIR "/usr/include" CACHE PATH "C header files for non-gcc (/usr/include)")
endif()

if(NOT DEFINED CMAKE_INSTALL_DATAROOTDIR)
  set(CMAKE_INSTALL_DATAROOTDIR "share" CACHE PATH "read-only architecture-independent data root (share)")
endif()

#-----------------------------------------------------------------------------
# Values whose defaults are relative to DATAROOTDIR.  Store empty values in
# the cache and store the defaults in local variables if the cache values are
# not set explicitly.  This auto-updates the defaults as DATAROOTDIR changes.

if(NOT CMAKE_INSTALL_DATADIR)
  set(CMAKE_INSTALL_DATADIR "" CACHE PATH "read-only architecture-independent data (DATAROOTDIR)")
  set(CMAKE_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "OpenBSD")
  if(NOT CMAKE_INSTALL_INFODIR)
    set(CMAKE_INSTALL_INFODIR "" CACHE PATH "info documentation (info)")
    set(CMAKE_INSTALL_INFODIR "info")
  endif()

  if(NOT CMAKE_INSTALL_MANDIR)
    set(CMAKE_INSTALL_MANDIR "" CACHE PATH "man documentation (man)")
    set(CMAKE_INSTALL_MANDIR "man")
  endif()
else()
  if(NOT CMAKE_INSTALL_INFODIR)
    set(CMAKE_INSTALL_INFODIR "" CACHE PATH "info documentation (DATAROOTDIR/info)")
    set(CMAKE_INSTALL_INFODIR "${CMAKE_INSTALL_DATAROOTDIR}/info")
  endif()

  if(NOT CMAKE_INSTALL_MANDIR)
    set(CMAKE_INSTALL_MANDIR "" CACHE PATH "man documentation (DATAROOTDIR/man)")
    set(CMAKE_INSTALL_MANDIR "${CMAKE_INSTALL_DATAROOTDIR}/man")
  endif()
endif()

if(NOT CMAKE_INSTALL_LOCALEDIR)
  set(CMAKE_INSTALL_LOCALEDIR "" CACHE PATH "locale-dependent data (DATAROOTDIR/locale)")
  set(CMAKE_INSTALL_LOCALEDIR "${CMAKE_INSTALL_DATAROOTDIR}/locale")
endif()

if(NOT CMAKE_INSTALL_DOCDIR)
  set(CMAKE_INSTALL_DOCDIR "" CACHE PATH "documentation root (DATAROOTDIR/doc/PROJECT_NAME)")
  set(CMAKE_INSTALL_DOCDIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${PROJECT_NAME}")
endif()

#-----------------------------------------------------------------------------

mark_as_advanced(
  CMAKE_INSTALL_BINDIR
  CMAKE_INSTALL_SBINDIR
  CMAKE_INSTALL_LIBEXECDIR
  CMAKE_INSTALL_SYSCONFDIR
  CMAKE_INSTALL_SHAREDSTATEDIR
  CMAKE_INSTALL_LOCALSTATEDIR
  CMAKE_INSTALL_LIBDIR
  CMAKE_INSTALL_INCLUDEDIR
  CMAKE_INSTALL_OLDINCLUDEDIR
  CMAKE_INSTALL_DATAROOTDIR
  CMAKE_INSTALL_DATADIR
  CMAKE_INSTALL_INFODIR
  CMAKE_INSTALL_LOCALEDIR
  CMAKE_INSTALL_MANDIR
  CMAKE_INSTALL_DOCDIR
  )

# Result directories
#
foreach(dir
    BINDIR
    SBINDIR
    LIBEXECDIR
    SYSCONFDIR
    SHAREDSTATEDIR
    LOCALSTATEDIR
    LIBDIR
    INCLUDEDIR
    OLDINCLUDEDIR
    DATAROOTDIR
    DATADIR
    INFODIR
    LOCALEDIR
    MANDIR
    DOCDIR
    )
  if(NOT IS_ABSOLUTE "${CMAKE_INSTALL_${dir}}")
    # Handle special cases:
    # - CMAKE_INSTALL_PREFIX == /
    # - CMAKE_INSTALL_PREFIX == /usr
    # - CMAKE_INSTALL_PREFIX == /opt/...
    if("${CMAKE_INSTALL_PREFIX}" STREQUAL "/")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR")
        set(CMAKE_INSTALL_FULL_${dir} "/${CMAKE_INSTALL_${dir}}")
      else()
        if (NOT "${CMAKE_INSTALL_${dir}}" MATCHES "^usr/")
          set(CMAKE_INSTALL_${dir} "usr/${CMAKE_INSTALL_${dir}}")
        endif()
        set(CMAKE_INSTALL_FULL_${dir} "/${CMAKE_INSTALL_${dir}}")
      endif()
    elseif("${CMAKE_INSTALL_PREFIX}" MATCHES "^/usr/?$")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR")
        set(CMAKE_INSTALL_FULL_${dir} "/${CMAKE_INSTALL_${dir}}")
      else()
        set(CMAKE_INSTALL_FULL_${dir} "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_${dir}}")
      endif()
    elseif("${CMAKE_INSTALL_PREFIX}" MATCHES "^/opt/.*")
      if("${dir}" STREQUAL "SYSCONFDIR" OR "${dir}" STREQUAL "LOCALSTATEDIR")
        set(CMAKE_INSTALL_FULL_${dir} "/${CMAKE_INSTALL_${dir}}${CMAKE_INSTALL_PREFIX}")
      else()
        set(CMAKE_INSTALL_FULL_${dir} "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_${dir}}")
      endif()
    else()
      set(CMAKE_INSTALL_FULL_${dir} "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_${dir}}")
    endif()
  else()
    set(CMAKE_INSTALL_FULL_${dir} "${CMAKE_INSTALL_${dir}}")
  endif()
endforeach()
