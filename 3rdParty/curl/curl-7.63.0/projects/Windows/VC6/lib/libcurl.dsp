# Microsoft Developer Studio Project File - Name="libcurl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Dynamic-Link Library" 0x0102
# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=libcurl - Win32 LIB Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "libcurl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "libcurl.mak" CFG="libcurl - Win32 LIB Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "libcurl - Win32 DLL Debug" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Debug DLL OpenSSL" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Debug DLL Windows SSPI" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Release" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Release DLL OpenSSL" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Release DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Release DLL Windows SSPI" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 DLL Release DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Dynamic-Link Library")
!MESSAGE "libcurl - Win32 LIB Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug DLL OpenSSL" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug DLL Windows SSPI" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug LIB OpenSSL" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release DLL OpenSSL" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release DLL Windows SSPI" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release LIB OpenSSL" (based on "Win32 (x86) Static Library")
!MESSAGE "libcurl - Win32 LIB Release LIB OpenSSL LIB LibSSH2" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""

!IF  "$(CFG)" == "libcurl - Win32 DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /FD /EHsc /GZ /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug\libcurld.dll" /pdbtype:con /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug\libcurld.dll" /pdbtype:con /fixed:no

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Debug DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_OPENSSL" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_OPENSSL" /FD /EHsc /GZ /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\libcurld.dll" /pdbtype:con /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\libcurld.dll" /pdbtype:con /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /fixed:no

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\libcurld.dll" /pdbtype:con /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Debug" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\libcurld.dll" /pdbtype:con /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Debug" /fixed:no

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Debug DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /FD /EHsc /GZ /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\libcurld.dll" /pdbtype:con /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\libcurld.dll" /pdbtype:con /fixed:no

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /GZ /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "_DEBUG" /win32
# ADD MTL /nologo /D "_DEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\libcurld.dll" /pdbtype:con /fixed:no
# ADD LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\libcurld.dll" /pdbtype:con /fixed:no

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /FD /EHsc /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Release DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_OPENSSL" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_OPENSSL" /FD /EHsc /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib /nologo /dll /pdb:none /machine:I386 /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /fixed:no /release

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Release DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /dll /pdb:none /machine:I386 /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Release" /fixed:no /release
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /dll /pdb:none /machine:I386 /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Release" /fixed:no /release

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Release DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /FD /EHsc /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release

!ELSEIF  "$(CFG)" == "libcurl - Win32 DLL Release DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\lib"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\lib"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /c
MTL=midl.exe
# ADD BASE MTL /nologo /D "NDEBUG" /win32
# ADD MTL /nologo /D "NDEBUG" /win32
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release
# ADD LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib /nologo /dll /pdb:none /machine:I386 /fixed:no /release

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug LIB OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "_DEBUG" /D "BUILDING_LIBCURL" /D "DEBUGBUILD" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /GZ /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\libcurld.lib" /machine:I386
# ADD LIB32 /nologo /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\libcurld.lib" /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_WINDOWS_SSPI" /D "USE_SCHANNEL" /D "USE_WIN32_IDN" /D "WANT_IDN_PROTOTYPES" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release LIB OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ELSEIF  "$(CFG)" == "libcurl - Win32 LIB Release LIB OpenSSL LIB LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\lib"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\lib"
# PROP Target_Dir ""
CPP=cl.exe
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\..\openssl\inc32" /I "..\..\..\..\..\libssh2\include" /D "NDEBUG" /D "BUILDING_LIBCURL" /D "CURL_STATICLIB" /D "USE_OPENSSL" /D "USE_LIBSSH2" /D "HAVE_LIBSSH2_H" /FD /EHsc /c
RSC=rc.exe
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /machine:I386
# ADD LIB32 /nologo /machine:I386

!ENDIF 

# Begin Target

# Name "libcurl - Win32 DLL Debug"
# Name "libcurl - Win32 DLL Debug DLL OpenSSL"
# Name "libcurl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2"
# Name "libcurl - Win32 DLL Debug DLL Windows SSPI"
# Name "libcurl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN"
# Name "libcurl - Win32 DLL Release"
# Name "libcurl - Win32 DLL Release DLL OpenSSL"
# Name "libcurl - Win32 DLL Release DLL OpenSSL DLL LibSSH2"
# Name "libcurl - Win32 DLL Release DLL Windows SSPI"
# Name "libcurl - Win32 DLL Release DLL Windows SSPI DLL WinIDN"
# Name "libcurl - Win32 LIB Debug"
# Name "libcurl - Win32 LIB Debug DLL OpenSSL"
# Name "libcurl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2"
# Name "libcurl - Win32 LIB Debug DLL Windows SSPI"
# Name "libcurl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN"
# Name "libcurl - Win32 LIB Debug LIB OpenSSL"
# Name "libcurl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2"
# Name "libcurl - Win32 LIB Release"
# Name "libcurl - Win32 LIB Release DLL OpenSSL"
# Name "libcurl - Win32 LIB Release DLL OpenSSL DLL LibSSH2"
# Name "libcurl - Win32 LIB Release DLL Windows SSPI"
# Name "libcurl - Win32 LIB Release DLL Windows SSPI DLL WinIDN"
# Name "libcurl - Win32 LIB Release LIB OpenSSL"
# Name "libcurl - Win32 LIB Release LIB OpenSSL LIB LibSSH2"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\lib\amigaos.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\asyn-ares.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\asyn-thread.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\base64.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\conncache.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\connect.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\content_encoding.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\cookie.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_addrinfo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ctype.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_des.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_endian.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_fnmatch.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_gethostname.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_gssapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_memrchr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_multibyte.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ntlm_core.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ntlm_wb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_path.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_range.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_rtmp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sasl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_threads.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\dict.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\doh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\dotdot.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\easy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\escape.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\file.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\fileinfo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\formdata.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ftp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ftplistparser.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\getenv.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\getinfo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\gopher.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hash.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hmac.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostasyn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostcheck.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostip4.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostip6.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostip.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostsyn.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_chunks.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_digest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_negotiate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_ntlm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_proxy.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\idn_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\if2ip.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\imap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\inet_ntop.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\inet_pton.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\krb5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ldap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\llist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\md4.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\md5.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\memdebug.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\mime.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\mprintf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\multi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\netrc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\non-ascii.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\nonblock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\openldap.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\parsedate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pingpong.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pipeline.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pop3.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\progress.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\psl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\rand.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\rtsp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\security.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\select.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\sendf.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\setopt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\sha256.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\share.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\slist.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\smb.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\smtp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\socks.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\socks_gssapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\socks_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\speedcheck.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\splay.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ssh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ssh-libssh.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strcase.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strdup.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strerror.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtok.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtoofft.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\system_win32.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\telnet.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\tftp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\timeval.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\transfer.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\urlapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\url.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\version.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\warnless.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\wildcard.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\x509asn1.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\cleartext.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\cram.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\digest.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\digest_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\krb5_gssapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\krb5_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\ntlm.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\ntlm_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\oauth2.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\spnego_gssapi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\spnego_sspi.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\vauth.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\cyassl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\darwinssl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\gskit.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\gtls.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\mbedtls.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\mesalink.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\nss.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\openssl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\polarssl.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\polarssl_threadlock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\schannel.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\schannel_verify.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\vtls.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\lib\amigaos.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\arpa_telnet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\asyn.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\config-win32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\conncache.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\connect.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\content_encoding.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\cookie.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_addrinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_base64.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ctype.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_des.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_endian.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_fnmatch.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_gethostname.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_gssapi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_hmac.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ldap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_md4.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_md5.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_memory.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_memrchr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_multibyte.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ntlm_core.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ntlm_wb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_path.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_printf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_range.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_rtmp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sasl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sec.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_setup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_setup_once.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sha256.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_sspi.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_threads.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curlx.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\dict.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\doh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\dotdot.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\easyif.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\escape.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\file.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\fileinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\formdata.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ftp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ftplistparser.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\getinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\gopher.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hash.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostcheck.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\hostip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http2.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_chunks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_digest.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_negotiate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_ntlm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\http_proxy.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\if2ip.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\imap.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\inet_ntop.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\inet_pton.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\llist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\memdebug.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\mime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\multihandle.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\multiif.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\netrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\non-ascii.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\nonblock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\parsedate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pingpong.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pipeline.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\pop3.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\progress.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\psl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\rand.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\rtsp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\select.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\sendf.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\setopt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\setup-vms.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\share.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\sigpipe.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\slist.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\smb.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\smtp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\sockaddr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\socks.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\speedcheck.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\splay.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\ssh.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strcase.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strdup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strerror.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtok.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtoofft.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\system_win32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\telnet.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\tftp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\timeval.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\transfer.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\urlapi-int.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\urldata.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\url.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\warnless.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\wildcard.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\x509asn1.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\digest.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\ntlm.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vauth\vauth.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\cyassl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\darwinssl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\gskit.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\gtls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\mbedtls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\mesalink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\nssg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\openssl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\polarssl.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\polarssl_threadlock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\schannel.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\vtls\vtls.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\lib\libcurl.rc
# End Source File
# End Group
# End Target
# End Project
