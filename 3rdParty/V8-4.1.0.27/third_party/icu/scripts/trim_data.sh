#!/bin/bash
# Copyright (c) 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


# Remove display names for languages that are not listed in the accept-language
# list of Chromium.
function filter_display_language_names {
  for lang in $(grep -v '^#' accept_lang.list)
  do
    # Set $OP to '|' only if $ACCEPT_LANG_PATTERN is not empty.
    OP=${ACCEPT_LANG_PATTERN:+|}
    ACCEPT_LANG_PATTERN="${ACCEPT_LANG_PATTERN}${OP}${lang}"
  done
  ACCEPT_LANG_PATTERN="(${ACCEPT_LANG_PATTERN})[^a-z]"

  echo "Filtering out display names for non-A-L languages ${langdatapath}"
  for lang in $(grep -v '^#' chrome_ui_languages.list)
  do
    target=${langdatapath}/${lang}.txt
    echo Overwriting ${target} ...
    sed -r -i \
    '/^    Keys\{$/,/^    \}$/d
     /^    Languages\{$/, /^    \}$/ {
       /^    Languages\{$/p
       /^        '${ACCEPT_LANG_PATTERN}'/p
       /^    \}$/p
       d
     }
     /^    Types\{$/,/^    \}$/d
     /^    Variants\{$/,/^    \}$/d' ${target}
  done
}


# Keep only the minimum locale data for non-UI languages.
function abridge_locale_data_for_non_ui_languages {
  for lang in $(grep -v '^#' chrome_ui_languages.list)
  do
    # Set $OP to '|' only if $UI_LANGUAGES is not empty.
    OP=${UI_LANGUAGES:+|}
    UI_LANGUAGES="${UI_LANGUAGES}${OP}${lang}"
  done

  EXTRA_LANGUAGES=$(egrep -v -e '^#' -e "(${UI_LANGUAGES})" accept_lang.list)

  echo Creating minimum locale data in ${localedatapath}
  for lang in ${EXTRA_LANGUAGES}
  do
    target=${localedatapath}/${lang}.txt
    [  -e ${target} ] || { echo "missing ${lang}"; continue; }
    echo Overwriting ${target} ...
    sed -n -r -i \
      '1, /^'${lang}'\{$/p
       /^    "%%ALIAS"\{/p
       /^    AuxExemplarCharacters\{.*\}$/p
       /^    AuxExemplarCharacters\{$/, /^    \}$/p
       /^    ExemplarCharacters\{.*\}$/p
       /^    ExemplarCharacters\{$/, /^    \}$/p
       /^    (LocaleScript|layout)\{$/, /^    \}$/p
       /^    Version\{.*$/p
       /^\}$/p' ${target}
  done

  echo Creating minimum locale data in ${langdatapath}
  for lang in ${EXTRA_LANGUAGES}
  do
    target=${langdatapath}/${lang}.txt
    [  -e ${target} ] || { echo "missing ${lang}"; continue; }
    echo Overwriting ${target} ...
    sed -n -r -i \
      '1, /^'${lang}'\{$/p
       /^    Languages\{$/, /^    \}$/ {
         /^    Languages\{$/p
         /^        '${lang}'\{.*\}$/p
         /^    \}$/p
       }
       /^\}$/p' ${target}
  done
}

# Drop historic currencies.
# TODO(jshin): Use ucurr_isAvailable in ICU to drop more currencies.
# See also http://en.wikipedia.org/wiki/List_of_circulating_currencies
function filter_currency_data {
  for currency in $(grep -v '^#' currencies_to_drop.list)
  do
    OP=${DROPLIST:+|}
    DROPLIST=${DROPLIST}${OP}${currency}
  done
  DROPLIST="(${DROPLIST})\{"

  cd "${dataroot}/curr"
  for i in *.txt
  do
    [ $i != 'supplementalData.txt' ] && \
    sed -r -i '/^        '$DROPLIST'/, /^        }/ d' $i
  done
}

# Remove the display names for numeric region codes other than
# 419 (Latin America) because we don't use them.
function filter_region_data {
  cd "${dataroot}/region"
  sed -i  '/[0-35-9][0-9][0-9]{/ d' *.txt
}



function remove_exemplar_cities {
  cd "${dataroot}/zone"
  for i in *.txt
  do
    [ $i != 'root.txt' ] && \
    sed -i '/^    zoneStrings/, /^        "meta:/ {
      /^    zoneStrings/ p
      /^        "meta:/ p
      d
    }' $i
  done
}

# Keep only duration and compound in units* sections.
function filter_locale_data {
  for i in ${dataroot}/locales/*.txt
  do
    echo Overwriting $i ...
    sed -r -i \
      '/^    units(|Narrow|Short)\{$/, /^    \}$/ {
         /^    units(|Narrow|Short)\{$/ p
         /^        (duration|compound)\{$/, /^        \}$/ p
         /^    \}$/ p
         d
       }' ${i}
  done
}

# big5han and gb2312han collation do not make any sense and nobody uses them.
function remove_legacy_chinese_codepoint_collation {
  echo "Removing Big5 / GB2312 collation data from Chinese locale"
  target="${dataroot}/coll/zh.txt"
  echo "Overwriting ${target}"
  sed -r -i '/^        (big5|gb2312)han\{$/,/^        \}$/ d' ${target}
}

dataroot="$(dirname $0)/../source/data"
localedatapath="${dataroot}/locales"
langdatapath="${dataroot}/lang"



filter_display_language_names
abridge_locale_data_for_non_ui_languages
filter_currency_data
filter_region_data
remove_legacy_chinese_codepoint_collation
filter_locale_data

# Chromium OS needs exemplar cities for timezones, but not Chromium.
# It'll save 400kB (uncompressed), but the size difference in
# 7z compressed installer is <= 100kB.
# TODO(jshin): Make separate data files for CrOS and Chromium.
#remove_exemplar_cities
