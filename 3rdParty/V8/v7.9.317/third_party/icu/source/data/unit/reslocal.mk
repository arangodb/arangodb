# © 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html#License
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
 en.txt en_001.txt en_150.txt\
 en_AU.txt en_CA.txt en_GB.txt en_IN.txt en_NZ.txt en_ZA.txt\
 en_AG.txt en_AI.txt en_AT.txt en_BB.txt en_BE.txt en_BM.txt\
 en_BS.txt en_BW.txt en_BZ.txt en_CC.txt en_CH.txt en_CK.txt\
 en_CM.txt en_CX.txt en_CY.txt en_DE.txt en_DG.txt en_DK.txt\
 en_DM.txt en_ER.txt en_FI.txt en_FJ.txt en_FK.txt en_FM.txt\
 en_GD.txt en_GG.txt en_GH.txt en_GI.txt en_GM.txt en_GY.txt\
 en_HK.txt en_IE.txt en_IL.txt en_IM.txt en_IO.txt\
 en_JE.txt en_JM.txt en_KE.txt en_KI.txt en_KN.txt en_KY.txt\
 en_LC.txt en_LR.txt en_LS.txt en_MG.txt en_MO.txt en_MS.txt\
 en_MT.txt en_MU.txt en_MW.txt en_MY.txt en_NA.txt en_NF.txt\
 en_NG.txt en_NH.txt en_NL.txt en_NR.txt en_NU.txt en_PG.txt\
 en_PH.txt en_PK.txt en_PN.txt en_PW.txt en_RH.txt en_RW.txt\
 en_SB.txt en_SC.txt en_SD.txt en_SE.txt en_SG.txt en_SH.txt\
 en_SI.txt en_SL.txt en_SS.txt en_SX.txt en_SZ.txt en_TC.txt\
 en_TK.txt en_TO.txt en_TT.txt en_TV.txt en_TZ.txt en_UG.txt\
 en_VC.txt en_VG.txt en_VU.txt en_WS.txt en_ZM.txt en_ZW.txt\
 es.txt es_419.txt es_AR.txt es_MX.txt es_US.txt\
 es_BO.txt es_BR.txt es_BZ.txt es_CL.txt es_CO.txt es_CR.txt\
 es_CU.txt es_DO.txt es_EC.txt es_GT.txt es_HN.txt es_NI.txt\
 es_PA.txt es_PE.txt es_PR.txt es_PY.txt es_SV.txt es_UY.txt es_VE.txt\
 et.txt\
 fa.txt\
 fi.txt\
 fil.txt\
 fr.txt fr_CA.txt fr_HT.txt\
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
 pt_AO.txt pt_CH.txt pt_CV.txt pt_GQ.txt pt_GW.txt pt_LU.txt\
 pt_MO.txt pt_MZ.txt pt_ST.txt pt_TL.txt\
 ro.txt ro_MD.txt\
 ru.txt\
 sk.txt\
 sl.txt\
 sr.txt sr_BA.txt sr_CS.txt sr_ME.txt sr_RS.txt sr_XK.txt\
 sr_Cyrl.txt sr_Cyrl_BA.txt sr_Cyrl_CS.txt sr_Cyrl_RS.txt sr_Cyrl_XK.txt\
 sr_Latn.txt sr_Latn_BA.txt sr_Latn_CS.txt sr_Latn_ME.txt sr_Latn_RS.txt\
 sv.txt\
 sw.txt\
 ta.txt\
 te.txt\
 th.txt\
 tr.txt\
 uk.txt\
 vi.txt\
 zh.txt zh_Hans.txt zh_Hans_CN.txt zh_Hans_SG.txt\
 zh_Hant.txt zh_Hant_TW.txt zh_Hant_HK.txt zh_Hant_MO.txt
