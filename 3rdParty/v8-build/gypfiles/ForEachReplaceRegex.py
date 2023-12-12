import re

def DoMain(args):
  pattern = args.pop(0)
  search = re.compile(pattern)
  replace = args.pop(0)
 
  #print("PATTERN: ", pattern)
  result = []
  for a in args:
    print("INPUT: ", a)
    replaced = search.sub(replace, a)
    print("OUTPUT: ", replaced)
    result.append(replaced)

  return ' '.join(result)
