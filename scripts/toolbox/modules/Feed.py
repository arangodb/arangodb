#!/usr/bin/env python

import subprocess


def start(cfg, operations):
    # Feed needs to have a local file as input
    fileName = "work/feedInput.txt"
    with open(fileName, "w") as text_file:
        text_file.write(operations)

    parameters = ["go", "run", "main.go", "--execute", "../../" + fileName, "--jsonOutputFile",
                  "../../" + cfg.feed["jsonOutputFile"]]
    process = subprocess.Popen(parameters, cwd="submodules/feed", stdout=subprocess.DEVNULL)
    return process
