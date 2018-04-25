import subprocess
import os.path
import json

def main():
    docsPath = os.path.normpath('../Documentation/Books/Manual/Programs')
    binPath = os.path.normpath('../build/bin')

    programs = [
        'arangobench',
        'arangod',
        'arangodump',
        'arangoexport',
        'arangoimp',
        'arangoimport',
        'arangorestore',
        'arangosh',
    ]

    for prog in programs:
        progPath = os.path.abspath(os.path.join(binPath, prog))
        if not os.path.exists(progPath):
            print('Executable not found:\n{}'.format(progPath))
            continue
        cmd = [prog, '--dump-options']
        proc = subprocess.Popen(cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )
        output, errors = proc.communicate()
        if len(errors) > 0:
            print('Errors during execution:\n\n{}'.format(errors))
            continue
        try:
            file = open(os.path.abspath(os.path.join(docsPath, prog.title(), 'Options.md')))
            file.write(generateTable(prog, output))
        except:
            print('TODO: Error handling')
        finally:
            file.close()

def generateTable(prog, output, docsPath):
    

main()