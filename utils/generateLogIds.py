#!/usr/bin/python3

import sys
import os

from enum import Enum
class Operation(Enum):
    READ = 1
    MODIFY = 2


import re
g_log_topic_pattern = re.compile("(?P<macro>LOG_TOPIC(_(RAW|IF|EVERY_N))?)\((?P<params>[^)]*)\)")

import hashlib
g_hash_algorithm = hashlib.md5()

def generate_id(location_string, params, id_database):
    id_len = 5
    g_hash_algorithm.update((location_string + params).encode())
    digest = g_hash_algorithm.hexdigest()
    uid = digest[0:id_len]

    while uid in id_database:
        print("collision on id {} \n{}\n{}\n".format(uid, location_string, id_database[uid]))
        g_hash_algorithm.update((location_string + digest).encode())
        digest = g_hash_algorithm.hexdigest()
        uid = digest[0:id_len]

    id_database[uid]=location_string

    return uid

def handle_file_cpp(fullpath, project_root, id_database, operation):
    project_path = os.path.relpath(fullpath, project_root)

    target_file = fullpath + ".replacement"
    replaced = False
    with open(target_file, 'w') as target_file_handle:
        with open(fullpath) as source_file_handle:
            status = 0
            for cnt, line in enumerate(source_file_handle):
                match = g_log_topic_pattern.search(line)
                if match:
                    location_string = "{}:{}".format(project_path, cnt)
                    macro = match.group('macro')
                    original_params = match.group('params')
                    splitted = [ x.strip() for x in original_params.split(',') ]

                    if macro.startswith("LOG_TOPIC"):
                        original_len = 2
                        modfied_len = 3

                        if macro == "LOG_TOPIC_IF" or macro == "LOG_TOPIC_EVERY_N":
                            original_len = 3
                            modfied_len = 4

                        #modify line to contain new id
                        if len(splitted) == original_len:
                            if (operation == Operation.READ):
                                continue
                            uid = generate_id(location_string, original_params, id_database)
                            replacement = '{}({}, {})'.format(macro,uid, original_params)
                            #output = "{} -- {}".format(location_string, replacement)
                            #print("REPLACE")
                            replaced = True
                            newline = g_log_topic_pattern.sub(replacement, line)
                            target_file_handle.write(newline)

                        #read modified line to build up id table
                        elif len(splitted) == modfied_len:
                            if (operation == Operation.MODIFY):
                                target_file_handle.write(line) #just copy
                                continue
                            uid = splitted[0].replace('"','')
                            if uid in id_database:
                                print("collision check failed - the following lines contain equal ids:")
                                print(location_string)
                                print(id_database[uid])
                                status = 1
                            else:
                                id_database[uid]=location_string


                        else:
                            print("BROKEN PARAMS IN:")
                            output = "{} -- {}".format(location_string, original_params)
                            print(output)
                            status = 1
                    else:
                        print("replacement for macro not implemented")
                        status = 1

                elif(operation == Operation.MODIFY):
                    target_file_handle.write(line)

                if status:
                    return status
    if replaced:
        os.rename(target_file, fullpath)
    else:
        os.unlink(target_file)


def generate_ids_main(project_root, directories_to_include, directories_or_files_to_exclude, id_database):
    include = [ project_root + os.path.sep + d for d in directories_to_include ]
    exclude = [ project_root + os.path.sep + d for d in directories_or_files_to_exclude ]

    for root, dirs, files in os.walk(project_root):
        status = 0
        dirs[:] = [ d for d in dirs if (root + os.path.sep + d).startswith(tuple(include)) ]
        if root in exclude:
            continue

        for operation in (Operation.READ, Operation.MODIFY):
            for filename in files:
                fullpath = root + os.path.sep + filename
                if fullpath in exclude:
                    continue
                if (filename.endswith((".cpp",".h"))):
                    status = handle_file_cpp(fullpath, project_root, id_database, operation)

                if status:
                    return status

    return 0

if __name__ == "__main__":
    root = os.path.dirname(__file__)
    root = os.path.abspath(root + os.path.sep + os.pardir)
    sys.exit(generate_ids_main(root                             #project_root
            ,["arangod", "arangosh", "enterprise", "lib"]        #dirs to include
            ,[".", "lib/Logger/LogMacros.h"]                    #dirs and files to exclude (does not purge the dir)
            , dict()                                            #database
            ));
