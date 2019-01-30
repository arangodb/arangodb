# ICU Data for Flutter

This directory contains the minimal ICU data configuration for the Flutter
framework (https://flutter.io). It is based on Chromium's iOS configuration
(see `../ios`) with additional data stripped out to reduce size:

 * All resources mentioned in `flutter-removed-resources.txt` are stripped out.
 * Additionally, `brkitr.patch` removes the break iterators for sentence and
   title breaks as well as the CSS specific line break iterators.

## Included Resources

 * Break iterators (and related dictionaries) for:
   * Characters
   * Words
   * Lines
 * Unicode Normalization Form KC (NFKC)
 * Likely Subtags


## Known Issues

 * https://github.com/flutter/flutter/issues/19584
