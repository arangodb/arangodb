# © 2016 and later: Unicode, Inc. and others.
# License & terms of use: http://www.unicode.org/copyright.html#License
GENRB_CLDR_VERSION = %version%
# A list of txt's to build
# The downstream packager may not need this file at all if their package is not
# constrained by
# the size (and/or their target OS already has ICU with the full locale data.)
#
# Listed below are locale data files necessary for 40 + 1 + 8 languages Chrome
# is localized to.
#
# In addition to them, 40+ "abridged" locale data files are listed. Chrome is
# localized to them, but
# uses a few categories of data in those locales for IDN handling and language
# name listing (in the UI).
# Aliases which do not have a corresponding xx.xml file (see icu-config.xml &
# build.xml)
GENRB_SYNTHETIC_ALIAS =

# All aliases (to not be included under 'installed'), but not including root.
GENRB_ALIAS_SOURCE = $(GENRB_SYNTHETIC_ALIAS)\
 zh_CN.txt zh_TW.txt zh_HK.txt zh_SG.txt\
 no.txt in.txt iw.txt tl.txt sh.txt

# Ordinary resources
GENRB_SOURCE =\
 am.txt\
 ar.txt ar_001.txt ar_AE.txt ar_BH.txt ar_DJ.txt\
 ar_DZ.txt ar_EG.txt ar_EH.txt ar_ER.txt ar_IL.txt\
 ar_IQ.txt ar_JO.txt ar_KM.txt ar_KW.txt ar_LB.txt\
 ar_LY.txt ar_MA.txt ar_MR.txt ar_OM.txt ar_PS.txt\
 ar_QA.txt ar_SA.txt ar_SD.txt ar_SO.txt ar_SS.txt\
 ar_SY.txt ar_TD.txt ar_TN.txt ar_YE.txt\
 bg.txt\
 bn.txt\
 ca.txt\
 cs.txt\
 da.txt\
 de.txt de_AT.txt de_BE.txt de_CH.txt de_IT.txt de_LI.txt de_LU.txt\
 el.txt\
 en.txt en_001.txt en_150.txt en_AG.txt en_AI.txt en_AS.txt\
 en_AT.txt en_AU.txt en_BB.txt en_BE.txt en_BI.txt\
 en_BM.txt en_BS.txt en_BW.txt en_BZ.txt en_CA.txt\
 en_CC.txt en_CH.txt en_CK.txt en_CM.txt en_CX.txt\
 en_CY.txt en_DE.txt en_DG.txt en_DK.txt en_DM.txt\
 en_ER.txt en_FI.txt en_FJ.txt en_FK.txt en_FM.txt\
 en_GB.txt en_GD.txt en_GG.txt en_GH.txt en_GI.txt\
 en_GM.txt en_GU.txt en_GY.txt en_HK.txt en_IE.txt\
 en_IL.txt en_IM.txt en_IN.txt en_IO.txt en_JE.txt\
 en_JM.txt en_KE.txt en_KI.txt en_KN.txt en_KY.txt\
 en_LC.txt en_LR.txt en_LS.txt en_MG.txt en_MH.txt\
 en_MO.txt en_MP.txt en_MS.txt en_MT.txt en_MU.txt\
 en_MW.txt en_MY.txt en_NA.txt en_NF.txt en_NG.txt\
 en_NL.txt en_NR.txt en_NU.txt en_NZ.txt en_PG.txt\
 en_PH.txt en_PK.txt en_PN.txt en_PR.txt en_PW.txt\
 en_RW.txt en_SB.txt en_SC.txt en_SD.txt en_SE.txt\
 en_SG.txt en_SH.txt en_SI.txt en_SL.txt en_SS.txt\
 en_SX.txt en_SZ.txt en_TC.txt en_TK.txt en_TO.txt\
 en_TT.txt en_TV.txt en_TZ.txt en_UG.txt en_UM.txt\
 en_US.txt en_US_POSIX.txt en_VC.txt en_VG.txt en_VI.txt\
 en_VU.txt en_WS.txt en_ZA.txt en_ZM.txt en_ZW.txt\
 es.txt es_ES.txt es_419.txt es_AR.txt es_MX.txt es_US.txt\
 es_BO.txt es_BR.txt es_BZ.txt es_CL.txt es_CO.txt es_CR.txt es_CU.txt\
 es_DO.txt es_EA.txt es_EC.txt es_GQ.txt es_GT.txt es_HN.txt es_IC.txt\
 es_NI.txt es_PA.txt es_PE.txt es_PH.txt es_PR.txt es_PY.txt es_SV.txt\
 es_UY.txt es_VE.txt\
 et.txt\
 fa.txt\
 fi.txt\
 fil.txt\
 fr.txt fr_BE.txt fr_BF.txt fr_BI.txt\
 fr_BJ.txt fr_BL.txt fr_CA.txt fr_CD.txt fr_CF.txt\
 fr_CG.txt fr_CH.txt fr_CI.txt fr_CM.txt fr_DJ.txt\
 fr_DZ.txt fr_FR.txt fr_GA.txt fr_GF.txt fr_GN.txt\
 fr_GP.txt fr_GQ.txt fr_HT.txt fr_KM.txt fr_LU.txt\
 fr_MA.txt fr_MC.txt fr_MF.txt fr_MG.txt fr_ML.txt\
 fr_MQ.txt fr_MR.txt fr_MU.txt fr_NC.txt fr_NE.txt\
 fr_PF.txt fr_PM.txt fr_RE.txt fr_RW.txt fr_SC.txt\
 fr_SN.txt fr_SY.txt fr_TD.txt fr_TG.txt fr_TN.txt\
 fr_VU.txt fr_WF.txt fr_YT.txt\
 gu.txt\
 he.txt\
 hi.txt\
 hr.txt\
 hu.txt\
 id.txt\
 it.txt it_CH.txt\
 ja.txt\
 kn.txt\
 ko.txt\
 lt.txt\
 lv.txt\
 ml.txt\
 mr.txt\
 ms.txt\
 nb.txt\
 nl.txt nl_AW.txt nl_BE.txt nl_BQ.txt nl_CW.txt nl_NL.txt\
 nl_SR.txt nl_SX.txt\
 pl.txt\
 pt.txt pt_AO.txt pt_BR.txt pt_CH.txt pt_CV.txt\
 pt_GQ.txt pt_GW.txt pt_LU.txt pt_MO.txt pt_MZ.txt\
 pt_PT.txt pt_ST.txt pt_TL.txt\
 ro.txt\
 ru.txt ru_BY.txt ru_KG.txt ru_KZ.txt ru_MD.txt ru_RU.txt ru_UA.txt\
 sk.txt\
 sl.txt\
 sr.txt sr_BA.txt sr_CS.txt sr_ME.txt sr_RS.txt sr_XK.txt\
 sr_Cyrl.txt sr_Cyrl_BA.txt sr_Cyrl_CS.txt sr_Cyrl_ME.txt\
 sr_Cyrl_RS.txt sr_Cyrl_XK.txt\
 sr_Latn.txt sr_Latn_BA.txt sr_Latn_CS.txt sr_Latn_ME.txt\
 sr_Latn_RS.txt sr_Latn_XK.txt\
 sv.txt\
 sw.txt sw_CD.txt sw_KE.txt\
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
 an.txt\
 ast.txt\
 az.txt\
 be.txt\
 bem.txt\
 br.txt\
 bs.txt\
 ckb.txt\
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
 ku.txt\
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
 tg.txt\
 ti.txt\
 to.txt\
 ur.txt\
 uz.txt\
 wa.txt\
 yo.txt\
 zu.txt
