MAN1 =
MAN3 =
MAN7 = czmq.7 \
    zclock.7 \
    zctx.7 \
    zfile.7 \
    zframe.7 \
    zhash.7 \
    zlist.7 \
    zloop.7 \
    zmsg.7 \
    zsocket.7 \
    zsockopt.7 \
    zstr.7 \
    zthread.7
MAN_DOC = $(MAN1) $(MAN3) $(MAN7)

MAN_TXT = $(MAN1:%.1=%.txt)
MAN_TXT += $(MAN3:%.3=%.txt)
MAN_TXT += $(MAN7:%.7=%.txt)

if INSTALL_MAN
dist_man_MANS = $(MAN_DOC)
endif

EXTRA_DIST = asciidoc.conf mkman $(MAN_TXT)

MAINTAINERCLEANFILES = $(MAN_DOC)

dist-hook : $(MAN_DOC)

if BUILD_DOC
SUFFIXES=.txt .xml .1 .3 .7

.txt.xml:
	./mkman $<
	asciidoc -d manpage -b docbook -f asciidoc.conf \
		-aczmq_version=@PACKAGE_VERSION@ $<
.xml.1:
	xmlto man $<
.xml.3:
	xmlto man $<
.xml.7:
	xmlto man $<
endif
