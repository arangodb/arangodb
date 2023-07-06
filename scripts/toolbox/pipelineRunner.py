#!/usr/bin/env python

from modules import Pipeline
import config as cfg
from modules import ArgumentParser

def main():
    args = ArgumentParser.arguments()
    calculatedConfig = cfg.CalculatedConfig(args).getConfig()
    Pipeline.executePipeline(calculatedConfig, args, "pipelines/pocPipeline.json")


if __name__ == "__main__":
    main()
