import os
import sys
import re
import inspect
import urllib
import io

validExtensions = (".cpp", ".h", ".js", ".md")
# specify the paths in which docublocks are searched. note that js/apps/* must not be included because it contains js/apps/system/
# and that path also contains copies of some files present in js/ anyway.

searchMDPaths = [
  "Manual",
  "AQL",
  "HTTP",
  "Cookbook",
  "Drivers"
]
searchPaths = [
  ["Documentation/Books/Manual/", False],
  ["Documentation/Books/AQL/", False],
  ["Documentation/Books/HTTP/", False],
  ["Documentation/Books/Cookbook/", False],
  ["Documentation/Books/Drivers/", False],
  ["Documentation/DocuBlocks/", True]
]
fullSuccess = True

def file_content(filepath, forceDokuBlockContent):
  """ Fetches and formats file's content to perform the required operation.
  """

  infile = io.open(filepath, 'r', encoding='utf-8', newline=None)
  filelines = tuple(infile)
  infile.close()

  comment_indexes = []
  comments = []
  _start = None
  docublockname = ""
  for line in enumerate(filelines):
    if "@startDocuBlock" in line[1]:
      docublockname = line[1]
      # in the unprocessed md files we have non-terminated startDocuBlocks, else it is an error:
      if ((_start != None) and
          (not searchMDPaths[0] in filepath) and
          (not searchMDPaths[1] in filepath) and
          (not searchMDPaths[2] in filepath) and
          (not searchMDPaths[3] in filepath) and
          (not searchMDPaths[4] in filepath)):
        print "next startDocuBlock found without endDocuBlock in between in file %s [%s]" %(filepath, line)
        raise Exception
      _start = line[0]
    if "@endDocuBlock" in line[1]:
      docublockname = ""
      try:
        _end = line[0] + 1
        comment_indexes.append([_start, _end])
        _start = None
      except NameError:
        print "endDocuBlock without previous startDocublock seen while analyzing file %s [%s]" %(filepath, line)
        raise Exception

  if len(docublockname) != 0 and forceDokuBlockContent: 
    print "no endDocuBlock found while analyzing file %s [%s]" %(filepath, docublockname)
    raise Exception

  for index in comment_indexes:
    comments.append(filelines[index[0]: index[1]])

  return comments


def example_content(filepath, fh, tag, blockType, placeIntoFilePath):
  """ Fetches an example file and inserts it using code
  """

  first = True
  aqlResult = False
  lastline = None
  longText = u""
  longLines = 0
  short = ""
  shortLines = 0
  shortable = False
  showdots = True

  CURL_STATE_CMD = 1
  CURL_STATE_HEADER = 2
  CURL_STATE_BODY = 3

  curlState = CURL_STATE_CMD

  AQL_STATE_QUERY = 1
  AQL_STATE_BINDV = 2
  AQL_STATE_RESULT = 3

  aqlState = AQL_STATE_QUERY
  blockCount = 0;

  # read in the context, split into long and short
  infile = io.open(filepath, 'r', encoding='utf-8', newline=None)
  for line in infile:
    if first:
      if blockType == "arangosh" and not line.startswith("arangosh&gt;"):
        raise Exception ("mismatching blocktype - expecting 'arangosh' to start with 'arangosh&gt' - in %s while inspecting %s - referenced via %s have '%s'" %(filepath, tag, placeIntoFilePath, line))
      if blockType == "curl" and not line.startswith("shell> curl"):
        raise Exception("mismatching blocktype - expecting 'curl' to start with 'shell > curl' in %s while inspecting %s - referenced via %s have '%s'" %(filepath, tag, placeIntoFilePath, line))
      first = False

    if blockType == "arangosh":
      if line.startswith("arangosh&gt;") or line.startswith("........&gt;"):
        if lastline != None:
          # short = short + lastline
          # shortLines = shortLines + 1
          lastline = None

        short += line
        shortLines += 1
        showdots = True
      else:
        if showdots:
          if lastline == None:
            # lastline = line
            shortable = True
            showdots = False
            lastline = None
          else:
            # short = short + "~~~hidden~~~\n"
            # shortLines = shortLines + 1
            shortable = True
            showdots = False
            lastline = None

    if blockType == "curl":
      if line.startswith("shell&gt; curl"):
        curlState = CURL_STATE_CMD
      elif curlState == CURL_STATE_CMD and line.startswith("HTTP/"):
        curlState = CURL_STATE_HEADER
      elif curlState == CURL_STATE_HEADER and line.startswith("{"):
        curlState = CURL_STATE_BODY

      if curlState == CURL_STATE_CMD or curlState == CURL_STATE_HEADER:
        short = short + line
        shortLines += 1
      else:
        shortable = True

    if blockType == "AQL":
      if line.startswith("@Q"): # query part
        blockCount = 0
        aqlState = AQL_STATE_QUERY
        short += "<strong>Query:</strong>\n<pre>\n"
        longText += "<strong>Query:</strong>\n<pre>\n"
        continue # skip this line - it is only here for this.
      elif line.startswith("@B"): # bind values part
        short += "</pre>\n<strong>Bind Values:</strong>\n<pre>\n"
        longText += "</pre>\n<strong>Bind Values:</strong>\n<pre>\n"
        blockCount = 0
        aqlState = AQL_STATE_BINDV
        continue # skip this line - it is only here for this.
      elif line.startswith("@R"): # result part
        shortable = True
        longText += "</pre>\n<strong>Results:</strong>\n<pre>\n"
        blockCount = 0
        aqlState = AQL_STATE_RESULT
        continue # skip this line - it is only here for this.

      if aqlState == AQL_STATE_QUERY or aqlState == AQL_STATE_BINDV:
        short = short + line
        shortLines += 1

      blockCount += 1

    longText += line
    longLines += 1

  if lastline != None:
    short += lastline
    shortLines += 1

  infile.close()

  if longLines - shortLines < 5:
    shortable = False
  # python3: urllib.parse.quote_plus

  # write example
  fh.write(unicode("\n"))

  utag = urllib.quote_plus(tag) + '_container'
  anchor = "<a name=\"" + utag + "\" id=\"" + utag + "\" class=\"anchor\" aria-hidden=\"true\" href=\"#" + utag + "\" >" + '''<svg class="svg-icon" viewBox="0 0 20 20"> <path fill="none" d="M19.175,4.856L15.138,0.82c-0.295-0.295-0.817-0.295-1.112,0L8.748,6.098c-0.307,0.307-0.307,0.805,0,1.112l1.462,1.462l-1.533,1.535L7.215,8.746c-0.307-0.307-0.805-0.307-1.112,0l-5.278,5.276c-0.307,0.307-0.307,0.805,0,1.112l4.037,4.037c0.154,0.153,0.355,0.23,0.556,0.23c0.201,0,0.403-0.077,0.556-0.23l5.28-5.276c0.148-0.148,0.23-0.347,0.23-0.556c0-0.209-0.083-0.409-0.23-0.556l-1.464-1.464l1.533-1.535l1.462,1.462c0.153,0.153,0.355,0.23,0.556,0.23c0.201,0,0.402-0.077,0.556-0.23l5.278-5.278c0.147-0.147,0.23-0.347,0.23-0.556C19.406,5.203,19.322,5.004,19.175,4.856zM9.585,13.339l-4.167,4.164l-2.925-2.925l4.166-4.164l0.906,0.905l-0.67,0.668c-0.307,0.307-0.307,0.805,0,1.112c0.154,0.153,0.356,0.23,0.556,0.23c0.203,0,0.403-0.077,0.556-0.23l0.67-0.668L9.585,13.339z M13.341,9.578l-0.906-0.906l0.663-0.662c0.307-0.307,0.307-0.805,0-1.112c-0.307-0.307-0.805-0.307-1.112,0L11.322,7.56l-0.906-0.906l4.166-4.166l2.925,2.925L13.341,9.578z"></path></svg></a>'''

  ustr = u"\uE9CB"
  # print(ustr)
  anchor =u"<a class=\"anchorjs-link \" href=\"#"+ utag + "\" aria-label=\"Anchor\" data-anchorjs-icon=\"" + ustr + "\" style=\"font: 1em/1 anchorjs-icons; padding-left: 0.375em;\"></a>"

  
#'''
#<h2>
# <a id="user-content-key-features-in-arangodb" class="anchor" aria-hidden="true" href="#key-features-in-arangodb">
#  <svg class="octicon octicon-link" viewBox="0 0 16 16" version="1.1" width="16" height="16" aria-hidden="true">
#    <path fill-rule="evenodd" d="M4 9h1v1H4c-1.5 0-3-1.69-3-3.5S2.55 3 4 3h4c1.45 0 3 1.69 3 3.5 0 1.41-.91 2.72-2 3.25V8.59c.58-.45 1-1.27 1-2.09C10 5.22 8.98 4 8 4H4c-.98 0-2 1.22-2 2.5S3 9 4 9zm9-3h-1v1h1c1 0 2 1.22 2 2.5S13.98 12 13 12H9c-.98 0-2-1.22-2-2.5 0-.83.42-1.64 1-2.09V6.25c-1.09.53-2 1.84-2 3.25C6 11.31 7.55 13 9 13h4c1.45 0 3-1.69 3-3.5S14.5 6 13 6z"></path>
#  </svg>
#</a>Key Features in ArangoDB</h2>
#'''
  
  longTag = "%s_long" % tag
  shortTag = "%s_short" % tag
  shortToggle = "$('#%s').hide(); $('#%s').show();" % (shortTag, longTag)
  longToggle = "$('#%s').hide(); $('#%s').show();" % (longTag, shortTag)

  if shortable:
    anchor += '<div id="%s_popup" onclick="%s">v</div><div id="%s_collapse" onclick="%s">^</div>' % (
      utag,
      shortToggle,
      utag,
      longToggle
      )
  longToggle = ""
  fh.write(unicode("<div id=\"%s\">\n" % utag))
  fh.write(unicode(anchor))


  if shortable:
    fh.write(unicode("<div id=\"%s\" onclick=\"%s\" style=\"Display: none;\">\n" % (longTag, longToggle)))
  else:
    fh.write(unicode("<div id=\"%s\">\n" % longTag))

  if blockType != "AQL":
    fh.write(unicode("<pre>\n"))
  fh.write(unicode("%s" % longText))
  fh.write(unicode("</pre>\n"))
  fh.write(unicode("</div>\n"))
  
  if shortable:
    fh.write(unicode("<div id=\"%s\" onclick=\"%s\">\n" % (shortTag, shortToggle)))
    if blockType != "AQL":
      fh.write(unicode("<pre>\n"))
    fh.write(unicode("%s" % short))

    if blockType == "arangosh":
      fh.write(unicode("</pre><div class=\"example_show_button\">show execution results</div>\n"))
    elif blockType == "curl":
      fh.write(unicode("</pre><div class=\"example_show_button\">show response body</div>\n"))
    elif blockType == "AQL":
      fh.write(unicode("</pre><div class=\"example_show_button\">show query result</div>\n"))
    else:
      fh.write(unicode("</pre><div class=\"example_show_button\">show</div>\n"))
      
    fh.write(unicode("</div>\n"))

  fh.write(unicode("</div>\n"))
  fh.write(unicode("\n"))


def fetch_comments(dirpath, forceDokuBlockContent):
  """ Fetches comments from files and writes to a file in required format.
  """
  global fullSuccess
  global validExtensions
  comments_filename = "allComments.txt"
  fh = io.open(comments_filename, "a", encoding="utf-8", newline="")
  shouldIgnoreLine = False

  for root, directories, files in os.walk(dirpath):
    for filename in files:
      if filename.endswith(validExtensions) and (filename.find("#") < 0):

        filepath = os.path.join(root, filename)
        file_comments = file_content(filepath, forceDokuBlockContent)
        for comment in file_comments:
          fh.write(unicode("\n<!-- filename: %s -->\n" % filepath))
          for _com in comment:
            _text = re.sub(r"//(/)+\s*\n", "<br />\n", _com) # place in temporary brs...
            _text = re.sub(r"///+(\s+\s+)([-\*\d])", r"  \2", _text)
            _text = re.sub(r"///\s", "", _text)
            _text = _text.strip("\n")
            if _text:
              if not shouldIgnoreLine:
                if ("@startDocuBlock" in _text) or \
                  ("@endDocuBlock" in _text):
                  fh.write(unicode("%s\n\n" % _text))
                elif ("@EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                      "@EXAMPLE_ARANGOSH_RUN" in _text or \
                      "@EXAMPLE_AQL" in _text):
                  blockType=""
                  if "@EXAMPLE_ARANGOSH_OUTPUT" in _text:
                    blockType = "arangosh"
                  elif "@EXAMPLE_ARANGOSH_RUN" in _text:
                    blockType = "curl"
                  elif "@EXAMPLE_AQL" in _text:
                    blockType = "AQL"

                  shouldIgnoreLine = True
                  try:
                    _filename = re.search("{(.*)}", _text).group(1)
                  except Exception as x:
                    print "failed to match file name in  %s while parsing %s " % (_text, filepath)
                    raise x
                  dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "Examples", _filename + ".generated"))
                  if os.path.isfile(dirpath):
                    example_content(dirpath, fh, _filename, blockType, filepath)
                  else:
                    fullSuccess = False
                    print "Could not find the generated example for " + _filename + " found in " + filepath
                else:
                  fh.write(unicode("%s\n" % _text))
              elif ("@END_EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                    "@END_EXAMPLE_ARANGOSH_RUN" in _text or \
                    "@END_EXAMPLE_AQL" in _text):
                shouldIgnoreLine = False
            else:
              fh.write(unicode("\n"))
  fh.close()

if __name__ == "__main__":
  errorsFile = io.open("../../lib/Basics/errors.dat", "r", encoding="utf-8", newline=None)
  commentsFile = io.open("allComments.txt", "w", encoding="utf-8", newline="")
  commentsFile.write(unicode("@startDocuBlock errorCodes \n"))
  for line in errorsFile:
    commentsFile.write(unicode(line + "\n"))
  commentsFile.write(unicode("@endDocuBlock \n"))
  commentsFile.close()
  errorsFile.close()
  for i in searchPaths:
    print "Searching for docublocks in " + i[0] + ": "
    dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i[0]))
    fetch_comments(dirpath, i[1])
    os.path.abspath(os.path.join(os.path.dirname( __file__ ), '..', 'templates'))
  if not fullSuccess:
    sys.exit(1)
