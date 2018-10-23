import sys
import re
import snowballstemmer


def usage():
    print("testapp.py <algorithm> \"sentence\"...")

def main():
    argv = sys.argv
    if len(argv) < 1:
        usage()
        return
    algorithm = 'english'
    if len(argv) > 2:
        algorithm = argv[1]
        argv = argv[2:]
    else:
        argv = argv[1:]
    stemmer = snowballstemmer.stemmer(algorithm)
    splitter = re.compile(r"[\s\.-]")
    for arg in argv:
        for word in splitter.split(arg):
            if word == '':
                continue
            original = word.lower()
            print(original + " -> " + stemmer.stemWord(original))
main()
