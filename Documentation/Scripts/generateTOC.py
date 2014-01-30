################################################################################
### @brief creates table-of-contents from documentation files
###
### @file
###
### DISCLAIMER
###
### Copyright by triAGENS GmbH - All rights reserved.
###
### The Programs (which include both the software and documentation)
### contain proprietary information of triAGENS GmbH; they are
### provided under a license agreement containing restrictions on use and
### disclosure and are also protected by copyright, patent and other
### intellectual and industrial property laws. Reverse engineering,
### disassembly or decompilation of the Programs, except to the extent
### required to obtain interoperability with other independently created
### software or as specified by law, is prohibited.
###
### The Programs are not intended for use in any nuclear, aviation, mass
### transit, medical, or other inherently dangerous applications. It shall
### be the licensee's responsibility to take all appropriate fail-safe,
### backup, redundancy, and other measures to ensure the safe use of such
### applications if the Programs are used for such purposes, and triAGENS
### GmbH disclaims liability for any damages caused by such use of
### the Programs.
###
### This software is the confidential and proprietary information of
### triAGENS GmbH. You shall not disclose such confidential and
### proprietary information and shall use it only in accordance with the
### terms of the license agreement you entered into with triAGENS GmbH.
###
### Copyright holder is triAGENS GmbH, Cologne, Germany
###
### @author Dr. Frank Celler
### @author Copyright 2011-2012, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string

argv = sys.argv
argv.pop(0)

DEBUG = False

print '# Auto-generated aliases'
print ''

################################################################################
### @brief parse file
################################################################################

r1 = re.compile(r'^- *@BOOK_REF{([^}]*)}')
r2 = re.compile(r'^@CHAPTER_REF{([^}]*)}')
r3 = re.compile(r'^.*{#([^}]*)}')

books = []
chapters = []
homes = {}

for filename in argv:
    f = open(filename, "r")
    num = 0
    superentry = ""

    for line in f:
        line = line.rstrip('\n')
        num = num + 1

        # first entry is home
        if num == 1:
            m = r3.match(line)

            if m:
                superentry = m.group(1)

        # books
        m = r1.match(line)

        if m:
            entry = m.group(1)
            books.append(entry)
            homes[entry] = 'Home'
            continue

        # chapters
        m = r2.match(line)

        if m:
            entry = m.group(1)
            chapters.append(entry)
            homes[entry] = superentry
            continue

    f.close()

def generate(l, h):
    for i in range(0,len(l)):
        entry = l[i]
        prev = ""
        next = ""
        home = ""

        if 0 < i:
            prev = l[i - 1]

        if i + 1 < len(l):
            next = l[i + 1]

        if entry in homes:
            home = homes[entry]

        if h:
            if prev != "" and home != homes[prev]:
                prev = ""

            if next != "" and home != homes[next]:
                next = ""

        if prev == "":
            print 'ALIASES += "NAVIGATE_%s=@NAVIGATE_FIRST{%s,%s}"' % (entry,home,next)
            print 'ALIASES += "BNAVIGATE_%s=@BNAVIGATE_FIRST{%s,%s}"' % (entry,home,next)
        elif next == "":
            print 'ALIASES += "NAVIGATE_%s=@NAVIGATE_LAST{%s,%s}"' % (entry,prev,home)
            print 'ALIASES += "BNAVIGATE_%s=@BNAVIGATE_LAST{%s,%s}"' % (entry,prev,home)
        else:
            print 'ALIASES += "NAVIGATE_%s=@NAVIGATE{%s,%s,%s}"' % (entry,prev,home,next)
            print 'ALIASES += "BNAVIGATE_%s=@BNAVIGATE{%s,%s,%s}"' % (entry,prev,home,next)

generate(books, None)
generate(chapters, homes)
