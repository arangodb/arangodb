import os
import re
import inspect


def file_content(filepath):
    """ Fetches and formats file's content to perform the required operation.
    """

    infile = open(filepath, 'r')
    filelines = tuple(infile)
    infile.close()

    comment_indexes = []
    comments = []

    for line in enumerate(filelines):
        if "@startDocuBlock" in line[1]:
            _start = line[0]
        if "@endDocuBlock" in line[1]:
            _end = line[0] + 1
            comment_indexes.append([_start, _end])

    for index in comment_indexes:
        comments.append(filelines[index[0]: index[1]])

    return comments


def example_content(filepath, fh, tag):
    """ Fetches an example file and inserts it using code
    """

    first = True
    arangosh = False
    showdots = True
    lastline = None
    short = ""
    shortLines = 0
    long = ""
    longLines = 0
    shortable = False

    # read in the context, split into long and short
    infile = open(filepath, 'r')

    for line in infile:
        if first:
            arangosh = line.startswith("arangosh>")
            first = False

        if arangosh:
            if line.startswith("arangosh>") or line.startswith("........>"):
                if lastline != None:
                    short = short + lastline
                    shortLines = shortLines + 1
                    lastline = None

                short = short + line
                showdots = True
            else:
                if showdots:
                    if lastline == None:
                        lastline = line
                    else:
                        short = short + "~~~hidden~~~\n"
                        shortLines = shortLines + 1
                        shortable = True
                        showdots = False
                        lastline = None

        long = long + line
        longLines = longLines + 1

    if lastline != None:
        short = short + lastline
        shortLines = shortLines + 1

    infile.close()

    if longLines - shortLines < 5:
        shortable = False

    # write example
    fh.write("\n")
    fh.write("<div id=\"%s_container\">\n" % tag)

    longTag = "%s_long" % tag
    shortTag = "%s_short" % tag

    longToggle = "$('#%s').hide(); $('#%s').show();" % (longTag, shortTag)
    shortToggle = "$('#%s').hide(); $('#%s').show();" % (shortTag, longTag)

    if shortable:
        fh.write("<div id=\"%s\" onclick=\"%s\" style=\"Display: none;\">\n" % (longTag, longToggle))
    else:
        fh.write("<div id=\"%s\">\n" % longTag)

    fh.write("<pre>\n")
    fh.write("```\n")
    fh.write("%s" % long)
    fh.write("```\n")
    fh.write("</pre>\n")
    fh.write("</div>\n")
    
    if shortable:
        fh.write("<div id=\"%s\" onclick=\"%s\">\n" % (shortTag, shortToggle))
        fh.write("<pre>\n")
        fh.write("```\n")
        fh.write("%s" % short)
        fh.write("```\n")
        fh.write("</pre>\n")
        fh.write("</div>\n")

    fh.write("</div>\n")


def fetch_comments(dirpath):
    """ Fetches comments from files and writes to a file in required format.
    """

    comments_filename = "allComments.txt"
    fh = open(comments_filename, "a")
    shouldIgnoreLine = False;

    for root, directories, files in os.walk(dirpath):
        for filename in files:
            filepath = os.path.join(root, filename)
            file_comments = file_content(filepath)
            for comment in file_comments:
                fh.write("\n<!-- filename: %s -->\n" % filename)
                for _com in comment:
                    _text = _com.replace("///", "")
                    if len(_text.strip()) == 0:
                        _text = _text.replace("\n", "<br />")    
                    _text = _text.strip()
                    if _text:
                      if not shouldIgnoreLine:
                        if ("@startDocuBlock" in _text) or \
                           ("@endDocuBlock" in _text):
                            fh.write("%s\n\n" % _text)
                        elif ("@EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                             "@EXAMPLE_ARANGOSH_RUN" in _text):
                          shouldIgnoreLine = True
                          _filename = re.search("{(.*)}", _text).group(1)
                          dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "Examples", _filename + ".generated"))
                          if os.path.isfile(dirpath):
                            example_content(dirpath, fh, _filename)
                          else:
                            print "Could not find code for " + _filename
                        else:
                            fh.write("%s\n" % _text)
                      elif ("@END_EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                           "@END_EXAMPLE_ARANGOSH_RUN" in _text):
                        shouldIgnoreLine = False

    fh.close()

if __name__ == "__main__":
    open("allComments.txt", "w").close()  
    path = ["arangod/cluster","arangod/RestHandler","arangod/V8Server","arangod/RestServer",
            "lib/Admin","lib/HttpServer",
            "js/actions","js/client","js/apps/databases","js/apps/system/cerberus","js/apps/system/gharial","js/common","js/server"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))
      fetch_comments(dirpath)
      print dirpath
      
      
      os.path.abspath(os.path.join(os.path.dirname( __file__ ), '..', 'templates'))
    
