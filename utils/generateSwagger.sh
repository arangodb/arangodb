#!/bin/bash

python \
  utils/generateExamples \
  . \
  js/apps/system/_admin/aardvark/APP/api-docs \
  api-docs \
  Documentation/DocuBlocks/Rest \
  > js/apps/system/_admin/aardvark/APP/api-docs.json
