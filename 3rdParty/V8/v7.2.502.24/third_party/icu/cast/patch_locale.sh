#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

treeroot="$(dirname "$0")/.."
cd "${treeroot}"

echo "Applying brkitr.patch"
patch -p1 < android/brkitr.patch || { echo "failed to patch" >&2; exit 1; }

# Keep only the currencies used by the larget 60 economies in terms of GDP
# with several more added in.
# TODO(jshin): Use ucurr_isAvailable in ICU to drop more currencies.
# See also http://en.wikipedia.org/wiki/List_of_circulating_currencies
# Copied from scripts/trim_data.sh. Need to refactor.
for currency in $(grep -v '^#' "${treeroot}/android/currencies.list")
do
  OP=${KEEPLIST:+|}
  KEEPLIST=${KEEPLIST}${OP}${currency}
done
KEEPLIST="(${KEEPLIST})"

cd source/data

for i in curr/*.txt
do
  locale=$(basename $i .txt)
  [ $locale == 'supplementalData' ] && continue;
  echo "Overwriting $i for $locale"
  sed -n -r -i \
    '1, /^'${locale}'\{$/ p
     /^    "%%ALIAS"\{/p
     /^    %%Parent\{/p
     /^    Currencies\{$/, /^    \}$/ {
       /^    Currencies\{$/ p
       /^        '$KEEPLIST'\{$/, /^        \}$/ p
       /^    \}$/ p
     }
     /^    Currencies%narrow\{$/, /^    \}$/ {
       /^    Currencies%narrow\{$/ p
       /^        '$KEEPLIST'\{".*\}$/ p
       /^    \}$/ p
     }
     /^    CurrencyPlurals\{$/, /^    \}$/ {
       /^    CurrencyPlurals\{$/ p
       /^        '$KEEPLIST'\{$/, /^        \}$/ p
       /^    \}$/ p
     }
     /^    [cC]urrency(Map|Meta|Spacing|UnitPatterns)\{$/, /^    \}$/ p
     /^    Version\{.*\}$/p
     /^\}$/p' $i

    # Delete empty blocks. Otherwise, locale fallback fails.
    # See crbug.com/791318 and crbug.com/870338 .
    sed -r -i \
      '/^    Currenc(ie.*|yPlurals)\{$/ {
         N
         /^    Currenc(ie.*|yPlurals)\{\n    \}/ d
      }' "${i}"
done


# Chrome on Android is not localized to the following languages and we
# have to minimize the locale data for them.
#EXTRA_LANGUAGES="bn et gu kn ml mr ms ta te"
EXTRA_LANGUAGES="gu kn ml mr ms ta te"

# TODO(jshin): Copied from scripts/trim_data.sh. Need to refactor.
echo Creating minimum locale data in locales
for lang in ${EXTRA_LANGUAGES}
do
  target=locales/${lang}.txt
  [  -e ${target} ] || { echo "missing ${lang}"; continue; }
  echo Overwriting ${target} ...

  # Do not include '%%Parent' line on purpose.
  sed -n -r -i \
    '1, /^'${lang}'\{$/p
     /^    "%%ALIAS"\{/p
     /^    (LocaleScript|layout)\{$/, /^    \}$/p
     /^    Version\{.*$/p
     /^\}$/p' ${target}
done

echo Creating minimum locale data in ${langdatapath}
for lang in ${EXTRA_LANGUAGES}
do
  target=lang/${lang}.txt
  [  -e ${target} ] || { echo "missing ${lang}"; continue; }
  echo Overwriting ${target} ...

  # Do not include '%%Parent' line on purpose.
  sed -n -r -i \
    '1, /^'${lang}'\{$/p
     /^    "%%ALIAS"\{/p
     /^    Languages\{$/, /^    \}$/ {
       /^    Languages\{$/p
       /^        '${lang}'\{.*\}$/p
       /^    \}$/p
     }
     /^\}$/p' ${target}
done

echo Overwriting curr/reslocal.mk to drop the currency names
echo for ${EXTRA_LANGUAGES}
for lang in ${EXTRA_LANGUAGES}
do
  sed -i -e '/'$lang'.txt/ d' curr/reslocal.mk
  sed -i -e '/'$lang'.txt/ d' zone/reslocal.mk
  sed -i -e '/'$lang'.txt/ d' unit/reslocal.mk
done

# Remove exemplar cities in timezone data.
# This is copied from scripts/trim_data.sh where it's disabled by default.
for i in zone/*.txt
do
  [ $i != 'zone/root.txt' ] && \
  sed -i '/^    zoneStrings/, /^        "meta:/ {
            /^    zoneStrings/ p
            /^        "meta:/ p
            d
          }' $i
done

# Keep only two common calendars. Add locale-specific calendars only to
# locales that are likely to use them most.
COMMON_CALENDARS="gregorian|generic"
for i in locales/*.txt; do
  CALENDARS="${COMMON_CALENDARS}"
  case $(basename $i .txt | sed 's/_.*$//') in
    th)
      EXTRA_CAL='buddhist'
      ;;
    zh)
      EXTRA_CAL='chinese'
      ;;
    ko)
      EXTRA_CAL='dangi'
      ;;
    am)
      EXTRA_CAL='ethiopic|ethiopic-amete-alem'
      ;;
    he)
      EXTRA_CAL='hebrew'
      ;;
    ar)
      # Other Islamic calendar formats are not in locales other than root.
      # ar-SA's default is islamic-umalqura, but its format entries are
      # specified in root via aliases.
      EXTRA_CAL='islamic'
      ;;
    fa)
      EXTRA_CAL='persian|islamic'
      ;;
    ja)
      EXTRA_CAL='japanese'
      ;;
    # When adding other Indian locales for Android,
    # add 'indian' calendar to them as well.
    hi)
      EXTRA_CAL='indian'
      ;;
    root)
      EXTRA_CAL='buddhist|chinese|roc|dangi|ethiopic|ethiopic-amete-alem|japanese|hebrew|islamic|islamic-(umalqura|civil|tbla|rgsa)|persian|indian'
      ;;
    *)
      EXTRA_CAL=''
      ;;
  esac

  # Add 'roc' calendar to zh_Hant*.
  [[ "$(basename $i .txt)" =~ 'zh_Hant' ]] && { EXTRA_CAL="${EXTRA_CAL}|roc"; }

  CAL_PATTERN="(${COMMON_CALENDARS}${EXTRA_CAL:+|${EXTRA_CAL}})"
  echo $CAL_PATTERN

  echo Overwriting $i...
  sed -r '/^    calendar\{$/,/^    \}$/ {
            /^    calendar\{$/p
            /^        default\{".*"\}$/p
            /^        '${CAL_PATTERN}'\{$/, /^        \}$/p
            /^    \}$/p
            d
          }' -i $i
done

# Delete Japanese era display names in root. 'ja' has Japanese era names
# so that root does not need them.
# The same is true of eras and monthNames for Islamic calendar.
sed -r -i \
  '/^        japanese\{$/,/^        \}$/ {
     /^            eras\{/,/^            \}$/d
   }
   /^        islamic\{$/,/^        \}$/ {
     /^            eras\{/,/^            \}$/d
     /^            monthNames\{/,/^            \}$/d
   }'  locales/root.txt

echo DONE.
