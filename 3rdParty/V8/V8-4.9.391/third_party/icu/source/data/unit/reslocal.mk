# *   Copyright (C) 1998-2014, International Business Machines
# *   Corporation and others.  All Rights Reserved.
UNIT_CLDR_VERSION = %version%
#
# A list of txt's to build
# The downstream packager may not need this file at all if their package is not
# constrained by
# the size (and/or their target OS already has ICU with the full locale data.)
#
# Listed below are locale data files necessary for 40 + 1 + 8 languages Chrome
# is localized to.
#
# Aliases which do not have a corresponding xx.xml file (see icu-config.xml &
# build.xml)
UNIT_SYNTHETIC_ALIAS =

# All aliases (to not be included under 'installed'), but not including root.
UNIT_ALIAS_SOURCE = $(UNIT_SYNTHETIC_ALIAS)\
 zh_CN.txt zh_TW.txt zh_HK.txt zh_SG.txt\
 no.txt in.txt iw.txt tl.txt sh.txt

# Ordinary resources
UNIT_SOURCE =\
 am.txt\
 ar.txt\
 bg.txt\
 bn.txt\
 ca.txt\
 cs.txt\
 da.txt\
 de.txt de_CH.txt\
 el.txt\
 en.txt en_001.txt en_GB.txt en_AU.txt en_NZ.txt en_ZA.txt\
 es.txt es_419.txt es_MX.txt es_AR.txt\
 et.txt\
 fa.txt\
 fi.txt\
 fil.txt\
 fr.txt fr_CA.txt\
 gu.txt\
 he.txt\
 hi.txt\
 hr.txt\
 hu.txt\
 id.txt\
 it.txt\
 ja.txt\
 kn.txt\
 ko.txt\
 lt.txt\
 lv.txt\
 ml.txt\
 mr.txt\
 ms.txt\
 nb.txt\
 nl.txt\
 pl.txt\
 pt.txt pt_PT.txt\
 ro.txt\
 ru.txt\
 sk.txt\
 sl.txt\
 sr.txt\
 sv.txt\
 sw.txt\
 ta.txt\
 te.txt\
 th.txt\
 tr.txt\
 uk.txt\
 vi.txt\
 zh.txt zh_Hans.txt zh_Hans_CN.txt zh_Hans_SG.txt\
 zh_Hant.txt zh_Hant_TW.txt zh_Hant_HK.txt\
 af.txt\
 ak.txt\
 az.txt\
 be.txt\
 bem.txt\
 br.txt\
 bs.txt\
 cy.txt\
 ee.txt\
 eo.txt\
 eu.txt\
 fo.txt\
 ga.txt\
 gl.txt\
 ha.txt\
 haw.txt\
 hy.txt\
 ig.txt\
 is.txt\
 ka.txt\
 kk.txt\
 km.txt\
 ky.txt\
 lg.txt\
 ln.txt\
 lo.txt\
 mfe.txt\
 mg.txt\
 mk.txt\
 mn.txt\
 mo.txt ro_MD.txt\
 mt.txt\
 my.txt\
 ne.txt\
 nn.txt\
 nyn.txt\
 om.txt\
 or.txt\
 pa.txt\
 ps.txt\
 rm.txt\
 rn.txt\
 rw.txt\
 si.txt\
 sn.txt\
 so.txt\
 sq.txt\
 sr_Latn.txt\
 ti.txt\
 to.txt\
 ur.txt\
 uz.txt\
 yo.txt\
 zu.txt
