#!/usr/bin/env python

import subprocess


def start(cfg, operations):
    parameters = [cfg.feed["executable"]]
    process = subprocess.Popen(parameters)
