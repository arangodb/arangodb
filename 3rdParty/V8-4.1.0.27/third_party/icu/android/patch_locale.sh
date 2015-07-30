#!/bin/sh
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

cd $(dirname $0)/../source/data

# Excludes curr data which is not used on Android.
echo Overwriting curr/reslocal.mk...
cat >curr/reslocal.mk <<END
CURR_CLDR_VERSION = 1.9
CURR_SYNTHETIC_ALIAS =
CURR_ALIAS_SOURCE =
CURR_SOURCE =
END

# Excludes region data. On Android Java API is used to get the data.
echo Overwriting region/reslocal.mk...
cat >region/reslocal.mk <<END
REGION_CLDR_VERSION = 1.9
REGION_SYNTHETIC_ALIAS =
REGION_ALIAS_SOURCE =
REGION_SOURCE =
END

# On Android Java API is used to get lang data, except for the language and
# script names for zh_Hans and zh_Hant which are not supported by Java API.
# Here remove all lang data except those names.
# See the comments in GetDisplayNameForLocale() (in Chromium's
# src/ui/base/l10n/l10n_util.cc) about why we need the scripts.
for i in lang/*.txt; do
  echo Overwriting $i...
  sed '/^    Keys{$/,/^    }$/d
       /^    Languages{$/,/^    }$/{
         /^    Languages{$/p
         /^        zh{/p
         /^    }$/p
         d
       }
       /^    LanguagesShort{$/,/^    }$/d
       /^    Scripts{$/,/^    }$/{
         /^    Scripts{$/p
         /^        Hans{/p
         /^        Hant{/p
         /^    }$/p
         d
       }
       /^    Types{$/,/^    }$/d
       /^    Variants{$/,/^    }$/d
       /^    calendar{$/,/^    }$/d
       /^    codePatterns{$/,/^    }$/d
       /^    localeDisplayPattern{$/,/^    }$/d' -i $i
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
      EXTRA_CAL='ethiopic'
      ;;
    he)
      EXTRA_CAL='hebrew'
      ;;
    ar)
      EXTRA_CAL='arabic'
      ;;
    fa)
      EXTRA_CAL='persian'
      ;;
    ja)
      EXTRA_CAL='japanese'
      ;;
  esac

  # Add 'roc' calendar to zh_Hant*.
  [[ "$(basename $i .txt)" =~ 'zh_Hant' ]] && { EXTRA_CAL="$EXTRA_CAL|roc"; }

  CAL_PATTERN="(${COMMON_CALENDARS}|${EXTRA_CAL})"
  echo $CAL_PATTERN

  echo Overwriting $i...
  sed -r '/^    calendar\{$/,/^    \}$/ {
            /^    calendar\{$/p
            /^        '${CAL_PATTERN}'\{$/, /^        \}$/p
            /^    \}$/p
            d
          }' -i $i
done

echo DONE.
