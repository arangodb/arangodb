# Note: A number of encodings are handled with purely algorithmic converters,
# without any mapping tables:
# US-ASCII, ISO 8859-1, UTF-7/8/16/32, SCSU

# Listed here:

# * ISO 8859-2..8,10,13,14,15,16
#   - 8859-11 table is not included. It's rather treated as a synonym of
#     Windows-874
# * Windows-125[0-8]
# * Simplified Chinese : GBK(Windows cp936), GB 18030
#   - GB2312 table was removed and 4 aliases for GB2312 were added
#     to GBK in convrtrs.txt to treat GB2312 as a synonym of GBK.
#   - GB-HZ is supported now that it uses the GBK table.
# * Traditional Chinese : Big5
# * Japanese : SJIS (shift_jis-html), EUC-JP (euc-jp-html)
# * Korean : EUC-KR per the encoding spec
# * Thai : Windows-874
#   - TIS-620 and ISO-8859-11 are treated as synonyms of Windows-874
#     although they're not the same.
# * Mac encodings : MacRoman, MacCyrillic
# * Cyrillic : KOI8-R, KOI8-U, IBM-866
#
# * Missing
#  - Armenian, Georgian  : extremly rare
#  - Mac encodings (other than Roman and Cyrillic) : extremly rare

UCM_SOURCE_FILES=


UCM_SOURCE_CORE=iso-8859-2-html.ucm iso-8859-3-html.ucm iso-8859-4-html.ucm\
iso-8859-5-html.ucm iso-8859-6-html.ucm iso-8859-7-html.ucm \
iso-8859-8-html.ucm iso-8859-10-html.ucm iso-8859-13-html.ucm \
iso-8859-14-html.ucm iso-8859-15-html.ucm iso-8859-16-html.ucm \
windows-1250-html.ucm windows-1251-html.ucm windows-1252-html.ucm\
windows-1253-html.ucm windows-1254-html.ucm windows-1255-html.ucm\
windows-1256-html.ucm windows-1257-html.ucm windows-1258-html.ucm\
windows-936-2000.ucm gb18030.ucm big5-html.ucm\
shift_jis-html.ucm euc-jp-html.ucm\
euc-kr-html.ucm\
windows-874-html.ucm \
macintosh-html.ucm x-mac-cyrillic-html.ucm\
ibm866-html.ucm koi8-r-html.ucm koi8-u-html.ucm



# Do not build EBCDIC converters.
# ibm-37 and ibm-1047 are hardcoded in Makefile.in and
# they're removed by modifying the file. It's also hard-coded in makedata.mak for
# Winwodws, but we don't have to touch it because the data dll is generated out of
# icu*.dat file generated on Linux.
UCM_SOURCE_EBCDIC =
