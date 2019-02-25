## Copyright (C) 2016 and later: Unicode, Inc. and others.
## License & terms of use: http://www.unicode.org/copyright.html#License
##
## Copyright (c) 2001-2009 International Business Machines
## Corporation and others. All Rights Reserved.
PACKAGE_NAME = uresb
TARGETS = en.res root.res sr.res

!IF "$(CFG)" == "x64\Release" || "$(CFG)" == "x64\Debug"
GENRB = ..\..\..\bin64\genrb.exe
!ELSE
GENRB = ..\..\..\bin\genrb.exe
!ENDIF

GENRBOPT = -s . -d .

all : $(TARGETS)
    @echo All targets are up to date

clean : 
    -erase $(TARGETS)

en.res : en.txt
    $(GENRB) $(GENRBOPT) $?

root.res : root.txt
    $(GENRB) $(GENRBOPT) $?

sr.res : sr.txt
    $(GENRB) $(GENRBOPT) --encoding cp1251 $?

