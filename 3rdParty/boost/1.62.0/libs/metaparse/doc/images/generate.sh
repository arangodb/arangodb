#!/bin/bash

for svg in *.svg
do
  rsvg-convert -o $(echo $svg | sed 's/\.svg/\.png/') -f png $svg
done

