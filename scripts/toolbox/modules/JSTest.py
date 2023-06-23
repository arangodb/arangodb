#!/usr/bin/env python

import subprocess
import os

def start(cfg, options):
    folderName = options["folderName"] + "/tests"
    for filename in os.listdir(folderName):
        // arangosh
        parameter = [cfg["arangosh"]["executable"], "--server.endpoint", cfg["endpoint"],
                     "--javascript.execute-string",
                     "const res = require('jsunity').runTest('", filename, "false);",
                     "require('@arangodb/testutils/result-processing').analyze.saveToJunitXML({testXmlOutputDirectory: '",
                     cfg["junitDirectory"], "'}, {transform: {poc: res}});"]
        print(parameter)
        process = subprocess.Popen(parameter, cwd="arangosh", stdout=subprocess.DEVNULL)
        return process
