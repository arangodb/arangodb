#!/usr/bin/env python

import subprocess


def start(cfg, operations):
    # Feed needs to have a local file as input
    fileName = cfg["globals"]["workDir"] + "/feedInput.txt"
    with open(fileName, "w") as textFile:
        textFile.write(operations)

    parameters = ["go", "run", "main.go", "--endpoints", cfg["feed"]["endpoints"], "--execute", "../" + fileName,
                  "--jsonOutputFile",
                  "../" + cfg["feed"]["jsonOutputFile"]]
    process = subprocess.Popen(parameters, cwd="feed", stdout=subprocess.DEVNULL)
    # TODO: instead of just putting it into DEVNULL, we can write output in a special verbose folder.
    return process


def startFromFile(cfg, fileName):
    parameters = ["go", "run", "main.go", "--endpoints", cfg["feed"]["endpoints"], "--execute", "../" + fileName,
                  "--jsonOutputFile",
                  "../" + cfg["feed"]["jsonOutputFile"]]
    process = subprocess.Popen(parameters, cwd="feed", stdout=subprocess.DEVNULL)
    # TODO: instead of just putting it into DEVNULL, we can write output in a special verbose folder.
    process.wait()
    return process
