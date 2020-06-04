from __future__ import unicode_literals
import os
import sys
import re
import inspect
import io
try:
  from urllib.parse import quote_plus
except ImportError:
  from urllib import quote_plus

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

  infile = io.open(filepath, encoding='utf-8', newline=None)
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
        print("next startDocuBlock found without endDocuBlock in between in file %s [%s]" %(filepath, line))
        raise Exception
      _start = line[0]
    if "@endDocuBlock" in line[1]:
      docublockname = ""
      try:
        _end = line[0] + 1
        comment_indexes.append([_start, _end])
        _start = None
      except NameError:
        print("endDocuBlock without previous startDocublock seen while analyzing file %s [%s]" %(filepath, line))
        raise Exception

  if len(docublockname) != 0 and forceDokuBlockContent: 
    print("no endDocuBlock found while analyzing file %s [%s]" %(filepath, docublockname))
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
  longText = ""
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
  blockCount = 0

  # read in the context, split into long and short
  infile = io.open(filepath, encoding='utf-8', newline=None)
  for line in infile:
    stripped_line = re.sub('<[^<]+?>', '', line) # remove HTML tags
    if first:
      if blockType == "arangosh" and not stripped_line.startswith("arangosh&gt;"):
        raise Exception ("mismatching blocktype - expecting 'arangosh' to start with 'arangosh&gt' - in %s while inspecting %s - referenced via %s have '%s'" %(filepath, tag, placeIntoFilePath, line))
      # HACK: highlight.js may add HTML to the prefix
      # <span class="hljs-meta">shell&gt;</span><span class="bash"> curl ...
      if blockType == "curl" and not stripped_line.startswith("shell&gt; curl"):
        raise Exception("mismatching blocktype - expecting 'curl' to start with 'shell> curl' in %s while inspecting %s - referenced via %s have '%s'" %(filepath, tag, placeIntoFilePath, line))
      first = False

    if blockType == "arangosh":
      if stripped_line.startswith("arangosh&gt;") or stripped_line.startswith("........&gt;"):
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
      if stripped_line.startswith("shell&gt; curl"):
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

    if blockType == "AQL" or blockType == "EXPLAIN":
      if line.startswith("@Q"): # query part
        blockCount = 0
        aqlState = AQL_STATE_QUERY
        if blockType == "EXPLAIN":
          short += "<strong>Explain Query:</strong>\n<pre>\n"
          longText += "<strong>Explain Query:</strong>\n<pre>\n"
        else:
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
        if blockType == "EXPLAIN":
          longText += "</pre>\n<strong>Explain output:</strong>\n<pre>\n"
        else:
          longText += "</pre>\n<strong>Query results:</strong>\n<pre>\n"
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
  fh.write("\n")

  utag = quote_plus(tag) + '_container'
  ustr = "\uE9CB"
  #anchor = "<a class=\"anchorjs-link \" href=\"#"+ utag + "\" aria-label=\"Anchor\" data-anchorjs-icon=\"" + ustr + "\"></a>"

  longTag = "%s_long" % tag
  shortTag = "%s_short" % tag
  shortToggle = "$('#%s').hide(); $('#%s').show();" % (shortTag, longTag)
  longToggle = "$('#%s').hide(); $('#%s').show(); window.location.hash='%s';" % (longTag, shortTag, utag)

  fh.write("<div class=\"example-container\" id=\"%s\">\n" % utag)
  #fh.write(anchor)

  if shortable:
    fh.write("<div id=\"%s\" style=\"display: none;\">\n" % longTag)
  else:
    fh.write("<div id=\"%s\">\n" % longTag)

  if blockType != "AQL" and blockType != "EXPLAIN":
    fh.write("<pre>\n")
  fh.write("%s" % longText)
  fh.write("</pre>\n")
  if shortable:
    hideText=""
    if blockType == "arangosh":
      hideText = "Hide execution results"
    elif blockType == "curl":
      hideText = "Hide response body"
    elif blockType == "AQL":
      hideText = "Hide query result"
    elif blockType == "EXPLAIN":
        hideText = "Hide explain output"
    else:
      hideText = "Hide"
    fh.write('<div id="%s_collapse" onclick="%s" class="example_show_button">%s</div>' % (
      utag,
      longToggle,
      hideText
      ))
  fh.write("</div>\n")
    
  if shortable:    
    fh.write("<div id=\"%s\" onclick=\"%s\">\n" % (shortTag, shortToggle))
    if blockType != "AQL" and blockType != "EXPLAIN":
      fh.write("<pre>\n")
    fh.write("%s" % short)

    if blockType == "arangosh":
      fh.write("</pre><div class=\"example_show_button\">Show execution results</div>\n")
    elif blockType == "curl":
      fh.write("</pre><div class=\"example_show_button\">Show response body</div>\n")
    elif blockType == "AQL":
      fh.write("</pre><div class=\"example_show_button\">Show query result</div>\n")
    elif blockType == "EXPLAIN":
      fh.write("</pre><div class=\"example_show_button\">Show explain output</div>\n")
    else:
      fh.write("</pre><div class=\"example_show_button\">Show</div>\n")
      
    fh.write("</div>\n")

  fh.write("</div>\n")
  fh.write("\n")


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
          fh.write("\n<!-- filename: %s -->\n" % filepath)
          explain = False
          for _com in comment:
            if "@EXPLAIN{TRUE}" in _com:
              explain = True
          for _com in comment:
            _text = re.sub(r"//(/)+\s*\n", "<br />\n", _com) # place in temporary brs...
            _text = re.sub(r"///+(\s+\s+)([-\*\d])", r"  \2", _text)
            _text = re.sub(r"///\s", "", _text)
            _text = _text.strip("\n")
            if _text:
              if not shouldIgnoreLine:
                if ("@startDocuBlock" in _text) or \
                  ("@endDocuBlock" in _text):
                  fh.write("%s\n\n" % _text)
                elif ("@EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                      "@EXAMPLE_ARANGOSH_RUN" in _text or \
                      "@EXAMPLE_AQL" in _text):
                  blockType=""
                  if "@EXAMPLE_ARANGOSH_OUTPUT" in _text:
                    blockType = "arangosh"
                  elif "@EXAMPLE_ARANGOSH_RUN" in _text:
                    blockType = "curl"
                  elif "@EXAMPLE_AQL" in _text:
                    if explain:
                      blockType = "EXPLAIN"
                    else:
                      blockType = "AQL"

                  shouldIgnoreLine = True
                  try:
                    _filename = re.search("{(.*)}", _text).group(1)
                  except Exception as x:
                    print("failed to match file name in  %s while parsing %s " % (_text, filepath))
                    raise x
                  dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "Examples", _filename + ".generated"))
                  if os.path.isfile(dirpath):
                    example_content(dirpath, fh, _filename, blockType, filepath)
                  else:
                    fullSuccess = False
                    print("Could not find the generated example for " + _filename + " found in " + filepath)
                else:
                  fh.write("%s\n" % _text)
              elif ("@END_EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                    "@END_EXAMPLE_ARANGOSH_RUN" in _text or \
                    "@END_EXAMPLE_AQL" in _text):
                shouldIgnoreLine = False
            else:
              fh.write("\n")
  fh.close()

if __name__ == "__main__":
  errorsFile = io.open("../../lib/Basics/errors.dat", encoding="utf-8", newline=None)
  commentsFile = io.open("allComments.txt", mode="w", encoding="utf-8", newline="")
  commentsFile.write("@startDocuBlock errorCodes \n")
  for line in errorsFile:
    commentsFile.write(line + "\n")
  commentsFile.write("@endDocuBlock \n")
  commentsFile.close()
  errorsFile.close()
  for i in searchPaths:
    print("Searching for docublocks in " + i[0] + ": ")
    dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i[0]))
    fetch_comments(dirpath, i[1])
    os.path.abspath(os.path.join(os.path.dirname( __file__ ), '..', 'templates'))
  if not fullSuccess:
    sys.exit(1)
