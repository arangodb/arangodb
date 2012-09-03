################################################################################
### @brief create a latex include file
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
### @author Copyright 2011, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, string

file_name = sys.argv[1]

DEBUG = False

################################################################################
### @brief parse file
################################################################################

r1 = re.compile(r'\\item \\doxyref{(.*)}{p.}{(.*)} *')
r2 = re.compile(r' *\\appendix *')

f = open(file_name, "r")

level = 0

for line in f:
    line = line.rstrip('\n')

    if line == '\\begin{DoxyEnumerate}':
        level = level + 1
    
    elif line == '\\end{DoxyEnumerate}':
        level = level - 1

    else:
        m = r1.match(line)

        if m:
            titel = m.group(1)
            inc = m.group(2)

            print "\\chapter{%s}\\label{%s}\\input{%s}" % (titel, inc, inc)

            continue
        #endif

        m = r2.match(line)

        if m:
            print "\\appendix"
        #endif
    #endif
#endfor

f.close()
