# Microsoft Developer Studio Project File - Name="curl" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=curl - Win32 LIB Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "curl.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "curl.mak" CFG="curl - Win32 LIB Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "curl - Win32 DLL Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Debug DLL OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Debug DLL Windows SSPI" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Release" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Release DLL OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Release DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Release DLL Windows SSPI" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 DLL Release DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug DLL OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug DLL Windows SSPI" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug LIB OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release DLL OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release DLL OpenSSL DLL LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release DLL Windows SSPI" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release DLL Windows SSPI DLL WinIDN" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release LIB OpenSSL" (based on "Win32 (x86) Console Application")
!MESSAGE "curl - Win32 LIB Release LIB OpenSSL LIB LibSSH2" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "curl - Win32 DLL Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug\src"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Debug DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL - DLL LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL OpenSSL" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Debug DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\DLL Debug - DLL Windows SSPI - DLL WinIDN" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release\src"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_CONSOLE" /D "NDEBUG" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Release DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Release DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL - DLL LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL OpenSSL" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Release DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 DLL Release DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN" /fixed:no
# ADD LINK32 wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\DLL Release - DLL Windows SSPI - DLL WinIDN" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug\src"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug\curl.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Debug" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL - DLL LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN" /fixed:no
# ADD LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib libcurld.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - DLL Windows SSPI - DLL WinIDN" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug LIB OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL" /fixed:no
# ADD LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD CPP /nologo /MDd /W4 /Zi /Od /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_DEBUG" /D "_CONSOLE" /D "DEBUGBUILD" /D "CURL_STATICLIB" /FD /EHsc /GZ /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\LIB Debug" /fixed:no
# ADD LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurld.lib libeay32.lib ssleay32.lib libssh2d.lib /nologo /subsystem:console /incremental:no /debug /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL - LIB LibSSH2\curld.exe" /pdbtype:con /libpath:"..\..\..\..\build\Win32\VC6\LIB Debug - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Debug" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\LIB Debug" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release\src"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "_CONSOLE" /D "NDEBUG" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release DLL OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release DLL OpenSSL DLL LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Release" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL - DLL LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\DLL Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\DLL Release" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release DLL Windows SSPI"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI" /fixed:no
# ADD LINK32 advapi32.lib wldap32.lib ws2_32.lib crypt32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release DLL Windows SSPI DLL WinIDN"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN" /fixed:no
# ADD LINK32 advapi32.lib normaliz.lib wldap32.lib ws2_32.lib crypt32.lib libcurl.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - DLL Windows SSPI - DLL WinIDN" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release LIB OpenSSL"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL" /fixed:no
# ADD LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Release" /fixed:no

!ELSEIF  "$(CFG)" == "curl - Win32 LIB Release LIB OpenSSL LIB LibSSH2"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2"
# PROP BASE Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\src"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2"
# PROP Intermediate_Dir "..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\src"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD CPP /nologo /MD /W4 /O2 /I "$(ProgramFiles)\Microsoft Platform SDK\Include" /I "..\..\..\..\include" /I "..\..\..\..\lib" /I "..\..\..\..\src" /D "NDEBUG" /D "_CONSOLE" /D "CURL_STATICLIB" /FD /EHsc /c
# ADD BASE RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
# ADD RSC /l 0x409 /i "..\..\..\..\include" /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\LIB Release" /fixed:no
# ADD LINK32 advapi32.lib crypt32.lib gdi32.lib user32.lib wldap32.lib ws2_32.lib libcurl.lib libeay32.lib ssleay32.lib libssh2.lib /nologo /subsystem:console /pdb:none /machine:I386 /out:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL - LIB LibSSH2\curl.exe" /libpath:"..\..\..\..\build\Win32\VC6\LIB Release - LIB OpenSSL" /libpath:"..\..\..\..\..\openssl\build\Win32\VC6\LIB Release" /libpath:"..\..\..\..\..\libssh2\build\Win32\VC6\LIB Release" /fixed:no

!ENDIF 

# Begin Target

# Name "curl - Win32 DLL Debug"
# Name "curl - Win32 DLL Debug DLL OpenSSL"
# Name "curl - Win32 DLL Debug DLL OpenSSL DLL LibSSH2"
# Name "curl - Win32 DLL Debug DLL Windows SSPI"
# Name "curl - Win32 DLL Debug DLL Windows SSPI DLL WinIDN"
# Name "curl - Win32 DLL Release"
# Name "curl - Win32 DLL Release DLL OpenSSL"
# Name "curl - Win32 DLL Release DLL OpenSSL DLL LibSSH2"
# Name "curl - Win32 DLL Release DLL Windows SSPI"
# Name "curl - Win32 DLL Release DLL Windows SSPI DLL WinIDN"
# Name "curl - Win32 LIB Debug"
# Name "curl - Win32 LIB Debug DLL OpenSSL"
# Name "curl - Win32 LIB Debug DLL OpenSSL DLL LibSSH2"
# Name "curl - Win32 LIB Debug DLL Windows SSPI"
# Name "curl - Win32 LIB Debug DLL Windows SSPI DLL WinIDN"
# Name "curl - Win32 LIB Debug LIB OpenSSL"
# Name "curl - Win32 LIB Debug LIB OpenSSL LIB LibSSH2"
# Name "curl - Win32 LIB Release"
# Name "curl - Win32 LIB Release DLL OpenSSL"
# Name "curl - Win32 LIB Release DLL OpenSSL DLL LibSSH2"
# Name "curl - Win32 LIB Release DLL Windows SSPI"
# Name "curl - Win32 LIB Release DLL Windows SSPI DLL WinIDN"
# Name "curl - Win32 LIB Release LIB OpenSSL"
# Name "curl - Win32 LIB Release LIB OpenSSL LIB LibSSH2"
# Begin Group "Source Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ctype.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\nonblock.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtoofft.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\warnless.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\slist_wc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_binmode.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_bname.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_dbg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_hdr.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_prg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_rea.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_see.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_wrt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cfgable.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_convert.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_dirhie.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_doswin.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_easysrc.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_filetime.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_formparse.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_getparam.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_getpass.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_help.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_helpers.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_homedir.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_hugehelp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_libinfo.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_main.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_metalink.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_msgs.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_operate.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_operhlp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_panykey.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_paramhlp.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_parsecfg.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_setopt.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_sleep.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_strdup.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_urlglob.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_util.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_vms.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_writeout.c
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_xattr.c
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\lib\config-win32.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_ctype.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\curl_setup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\nonblock.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\strtoofft.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\lib\warnless.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\slist_wc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_binmode.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_bname.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_dbg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_hdr.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_prg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_rea.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_see.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cb_wrt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_cfgable.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_convert.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_dirhie.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_doswin.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_easysrc.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_filetime.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_formparse.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_getparam.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_getpass.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_helpers.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_help.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_homedir.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_hugehelp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_libinfo.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_main.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_metalink.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_msgs.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_operate.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_operhlp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_panykey.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_paramhlp.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_parsecfg.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_sdecls.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_setopt.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_setup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_sleep.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_strdup.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_urlglob.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_util.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_version.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_vms.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_writeout.h
# End Source File
# Begin Source File

SOURCE=..\..\..\..\src\tool_xattr.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter ""
# Begin Source File

SOURCE=..\..\..\..\src\curl.rc
# End Source File
# End Group
# End Target
# End Project
