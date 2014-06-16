import os
import re
import inspect


def file_content(filepath):
    """ Fetches and formats file's content to perform the required operation.
    """

    filelines = tuple(open(filepath, 'r'))

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

def example_content(filepath, fh):
  """ Fetches an example file and inserts it using code
  """

  fh.write("\n```\n")

  filelines = tuple(open(filepath, 'r'))
  for line in enumerate(filelines):
    fh.write("%s" % line[1])

  fh.write("```\n")

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
                            fh.write("<!-- %s -->\n\n" % _text)
                        elif ("@EXAMPLE_ARANGOSH_OUTPUT" in _text or \
                            "@EXAMPLE_ARANGOSH_RUN" in _text):
                          shouldIgnoreLine = True
                          _filename = re.search("{(.*)}", _text).group(1)
                          dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir, "Examples", _filename + ".generated"))
                          if os.path.isfile(dirpath):
                            example_content(dirpath, fh)
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
    path = ["arangod/cluster","arangod/RestHandler","arangod/V8Server",
            "lib/Admin","lib/HttpServer",
            "js/actions","js/client","js/apps","js/common","js/server"]
    for i in path:
      dirpath = os.path.abspath(os.path.join(os.path.dirname( __file__ ), os.pardir,"ArangoDB/../../"+i))
      fetch_comments(dirpath)
      print dirpath
      
      
      os.path.abspath(os.path.join(os.path.dirname( __file__ ), '..', 'templates'))
    
