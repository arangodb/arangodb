################################################################################
### @brief Doxygen XML to Markdown
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
### @author Dr. Wolfgang Helisch
### @author Copyright 2011, triagens GmbH, Cologne, Germany
################################################################################

import re, sys, xml.parsers.expat

file_name = sys.argv[1]

DEBUG = False

if DEBUG:
    print "========== %s ==========" % file_name

## -----------------------------------------------------------------------------
## --SECTION--                                                  global variables
## -----------------------------------------------------------------------------

################################################################################
#### @brief table format
################################################################################

table_format = 'style="border-collapse: collapse; border-width: 1px; border-style: solid; border-color: #000"'

################################################################################
#### @brief table entry format
################################################################################

entry_format = 'style="border-style: solid; border-width: 1px"'

################################################################################
#### @brief replacement dictionary
################################################################################

replDict = {}

zero_tag_names = [
    'compounddef', 'compoundname', 'copydoc', 'detaileddescription', 'doxygen'
]

for tag_name in zero_tag_names:
   replDict["s_%s" % tag_name] = ""
   replDict["e_%s" % tag_name] = ""
#endfor

replDict["s_anchor"] = "<a name=\"%s\">"
replDict["e_anchor"] = "</a>\n"

replDict["s_bold"] = "<b>"
replDict["e_bold"] = "</b>"

replDict["s_computeroutput"] = "<tt>"
replDict["e_computeroutput"] = "</tt>"

replDict["s_emphasis"] = "<em>"
replDict["e_emphasis"] = "</em>"

replDict["s_entry"] = "\n|%s|" % entry_format
replDict["e_entry"] = ""

replDict["s_linebreak"] = ""
replDict["e_linebreak"] = "\n"

replDict["s_hruler"] = "<hr>"
replDict["e_hruler"] = "\n"

replDict["s_itemizedlist"] = "<ul>"
replDict["e_itemizedlist"] = "</ul>"

replDict["s_orderedlist"] = "<ol>"
replDict["e_orderedlist"] = "</ol>"

replDict["s_listitem_1"] = "<li>"
replDict["e_listitem_1"] = "</li>"

replDict["s_listitem_2"] = "<li>"
replDict["e_listitem_2"] = "</li>"

replDict["s_listitem_3"] = "<li>"
replDict["e_listitem_3"] = "</li>"

replDict["s_listitem_4"] = "<li>"
replDict["e_listitem_4"] = "</li>"

replDict["s_listitem_5"] = "<li>"
replDict["e_listitem_5"] = "</li>"

replDict["s_xrefsect"] = ""
replDict["e_xrefsect"] = ""

replDict["s_xreftitle"] = ""
replDict["e_xreftitle"] = "\n"

replDict["s_xrefdescription"] = ""
replDict["e_xrefdescription"] = ""

replDict["s_para"] = ""
replDict["e_para"] = "\n\n"

replDict["s_ref"] = "__~4__"
replDict["e_ref"] = "__~5__"

replDict["s_row"] = "\n|-\n"
replDict["e_row"] = ""

replDict["s_sect1"] = ""
replDict["s_sect2"] = ""
replDict["s_sect3"] = ""
replDict["e_sect1"] = ""
replDict["e_sect2"] = ""
replDict["e_sect3"] = ""

replDict["s_table"] = "\n{| %s\n" % table_format
replDict["e_table"] = "\n|}\n"

replDict["s_title_1"] = "# "
replDict["s_title_2"] = "## "
replDict["s_title_3"] = "### "
replDict["s_title_4"] = "#### "
replDict["e_title_1"] = "\n\n"
replDict["e_title_2"] = "\n\n"
replDict["e_title_3"] = "\n\n"
replDict["e_title_4"] = "\n\n"

replDict["s_ulink"] = "["
replDict["e_ulink"] = "]"

replDict["s_verbatim"] = ""
replDict["e_verbatim"] = ""

replDict["s_simplesect"] = ""
replDict["e_simplesect"] = ""

replDict["s_htmlonly"] = ""
replDict["e_htmlonly"] = ""

replDict["s_xmlonly"] = ""
replDict["e_xmlonly"] = ""

replDict["s_latexonly"] = ""
replDict["e_latexonly"] = ""

replDict["s_nonbreakablespace"] = " "
replDict["e_nonbreakablespace"] = ""

replDict["s_programlisting"] = "<pre>"
replDict["e_programlisting"] = "</pre>"

replDict["s_codeline"] = ""
replDict["e_codeline"] = ""

replDict["s_highlight"] = ""
replDict["e_highlight"] = ""

replDict["s_sp"] = " "
replDict["e_sp"] = ""

################################################################################
#### @brief generate code for text value
################################################################################

gencDict = {}

for tag_name in zero_tag_names:
   gencDict["%s" % tag_name] = False
#endfor

gencDict["anchor"] = True

gencDict["bold"] = True

gencDict["computeroutput"] = True

gencDict["emphasis"] = True

gencDict["entry"] = True

gencDict["linebreak"] = True
gencDict["hruler"] = False

gencDict["itemizedlist"] = False
gencDict["orderedlist"] = False
gencDict["listitem_1"] = False
gencDict["listitem_2"] = False
gencDict["listitem_3"] = False
gencDict["listitem_4"] = False
gencDict["listitem_5"] = False

gencDict["para"] = True

gencDict["ref"] = True

gencDict["row"] = True

gencDict["sect1"] = False
gencDict["sect2"] = False
gencDict["sect3"] = False

gencDict["table"] = True

gencDict["title_1"] = True
gencDict["title_2"] = True
gencDict["title_3"] = True
gencDict["title_4"] = True

gencDict["ulink"] = True

gencDict["verbatim"] = True

gencDict["simplesect"] = True

gencDict["xrefsect"] = True
gencDict["xreftitle"] = True
gencDict["xrefdescription"] = True

gencDict["htmlonly"] = False
gencDict["latexonly"] = False
gencDict["xmlonly"] = True

gencDict["nonbreakablespace"] = True

gencDict["programlisting"] = True

gencDict["codeline"] = True

gencDict["highlight"] = True

gencDict["sp"] = False

################################################################################
#### @brief table entry
################################################################################

tabentry = False

################################################################################
#### @brief after titel
################################################################################

titafter = False

################################################################################
#### @brief verbatim
################################################################################

verbatim = False

################################################################################
#### @brief xml path
################################################################################

path = []

################################################################################
#### @brief stack for generate code flags
################################################################################

gencode = []

################################################################################
#### @brief titel level (sect1, sect2, sect3)
################################################################################

titlevel = 1

################################################################################
#### @brief list level
################################################################################

listlevel = 0

################################################################################
#### @brief generated text
################################################################################

text_string = ''

## -----------------------------------------------------------------------------
## --SECTION--                                                  global functions
## -----------------------------------------------------------------------------

################################################################################
### @brief convert link
################################################################################

def convert_link(link, anchor):
    link = re.sub('^.*/(.*)$', '\\1', link)

    if anchor:
        link = re.sub('.*_', '', link)
    else:
        link = re.sub('_', '#wiki-', link)
        
    return link

################################################################################
### @brief start element
################################################################################

def start_element(name, attrs):
    global curlevel, tabentry, text_string, titafter, titlevel, verbatim, listlevel, DEBUG
    
    path.append(name)
    
    if   name == "sect1": titlevel = 2
    elif name == "sect2": titlevel = 3
    elif name == "sect3": titlevel = 4
    elif name == "entry": tabentry = True
    elif name == "verbatim": verbatim = True
    elif name == "itemizedlist": listlevel = listlevel + 1
    elif name == "orderedlist": listlevel = listlevel + 1
    #endif
    
    text = ""

    if name == "title":
        text += replDict["s_%s_%d" % (name, titlevel)]
        genc = gencDict["%s_%d" % (name, titlevel)]
    elif name == "listitem":
        text += replDict["s_%s_%d" % (name, listlevel)]
        genc = gencDict["%s_%d" % (name, listlevel)]
    elif name == "anchor":
        text += replDict["s_%s" % name] % convert_link(attrs['id'], True)
        genc = gencDict["%s" % name]
    else:
        text += replDict["s_%s" % name]
        genc = gencDict["%s" % name]
    #endif

    if name == "sect1" or name == "sect2" or name == "sect3":
        text += "<a name=\"%s\"></a>\n\n" % convert_link(attrs['id'], True)

    gencode.append(genc)
    
    if name == "ref": 
        text += "%s__~6__" % convert_link(attrs['refid'], False)
    #endif

    if name == "ulink":
        text += "__~9__%s__~10__" % attrs['url']
    #endif

    if DEBUG:
        print ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"
        print "PATH: %s" % path
        print "LIST: %d" % listlevel
        print "TITL: %d" % titlevel
        print "TEXT: <%s>" % text
        print ">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>"

    text_string += text
#enddef

################################################################################
### @brief end element
################################################################################

def end_element(name):
    global curlevel, tabentry, text_string, titafter, titlevel, verbatim, listlevel, DEBUG
    
    last = path[-1]

    path.pop()
    gencode.pop()
    
    if   name == "sect1": titlevel = 1
    elif name == "sect2": titlevel = 2
    elif name == "sect3": titlevel = 3
    elif name == "listitem": listitem = False
    elif name == "entry": tabentry = False
    elif name == "verbatim": verbatim = False
    elif name == "itemizedlist": listlevel = listlevel - 1
    elif name == "orderedlist": listlevel = listlevel - 1
    #endif
    
    if name == "title":
        titafter = True
        text = replDict["e_%s_%d" % (name, titlevel)]
    elif name == "listitem":
        text = replDict["e_%s_%d" % (name, listlevel)]
    else:
        text = replDict["e_%s" % name]
    #endif
    
    if 0 < len(path) and last == "para" and path[-1] == "listitem":
        text = ""
    #endif

    if DEBUG:
        print "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"
        print "PATH: %s" % path
        print "LIST: %d" % listlevel
        print "TITL: %d" % titlevel
        print "TEXT: <%s>" % text
        print "<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<"

    text_string += text
#enddef

################################################################################
### @brief handle text
################################################################################

def char_data(data):
    global text_string, DEBUG
    
    if len(gencode) == 0 or not gencode[-1]:
        return

    text = repr(data)
    text = text[1 : len(text) - 1 ]
    
    text = text.replace("\\'", "'")
    text = text.replace("\\\\n", "__~8__")
    text = text.replace("\\n", "\n")
    text = text.replace("\\\\", "\\")
    text = text.replace("__~8__", "\\\\n")
   
    if verbatim:
        text = text.replace("\n", "__~2____~1__")
        text = "__~1__%s__~3__" % text
        text = text.replace("__~1____~3__", "")
    #endif

    if DEBUG:
        print "--------------------------------------------------------------------------------"
        print "PATH: %s" % path
        print "TEXT: <%s>" % text
        print "--------------------------------------------------------------------------------"

    text_string += text
#enddef

################################################################################
### @brief finalize string
################################################################################

def finalize_string(text):

    # function header
    text = re.sub('^[ \t]*<tt><b>(.*)</b></tt>$', '<b>\\1</b>', text, 0, re.MULTILINE)
    
    # links
    text = re.sub('__~4__([^~/]*)__~6__([^~]*)__~5__', '<a href="\\1">\\2</a>', text)
    text = re.sub('\\[__~9__([^~]*)__~10__([^\\]]*)\\]', '[\\2](\\1)', text)

    # verbatim
    text = text.replace("__~1__", "    ")
    text = text.replace("__~2__", "\n")
    
    # really ugly, but fixes things that otherwise go wrong
    # unfortunately we can't simply replace <>&"' in text values as this is wrong, either
    # text = text.replace("<tt><", "<tt>&lt;")
    text = text.replace("<tt><b>", "__GRRR__")
    text = text.replace("<tt><", "<tt>&lt;")
    text = text.replace("__GRRR__", "<tt><b>")
    
    return text
#enddef

################################################################################
### @brief parse file
################################################################################

p = xml.parsers.expat.ParserCreate()

p.buffer_text = True
p.returns_unicode = False
p.StartElementHandler = start_element
p.EndElementHandler = end_element
p.CharacterDataHandler = char_data

f = open(file_name, "r")
p.ParseFile(f)
f.close()

print "%s" % (finalize_string(text_string))
