// Internal to the library, do not include this directly

#pragma once

#include "unicorn/utility.hpp"
#include <cstdint>

namespace RS::Unicorn {

    RS_ENUM_CLASS(Bidi_Class, int, 0,
        Default, AL, AN, B, BN, CS, EN, ES, ET, FSI, L, LRE, LRI, LRO, NSM,
        ON, PDF, PDI, R, RLE, RLI, RLO, S, WS
    )

    RS_ENUM_CLASS(East_Asian_Width, int, 0,
        N, A, F, H, Na, W
    )

    RS_ENUM_CLASS(Grapheme_Cluster_Break, int, 0,
        Other, Control, CR, EOT, Extend, L, LF, LV, LVT, Prepend,
        Regional_Indicator, SOT, SpacingMark, T, V
    )

    RS_ENUM_CLASS(Hangul_Syllable_Type, int, 0,
        NA, L, LV, LVT, T, V
    )

    RS_ENUM_CLASS(Indic_Positional_Category, int, 0,
        NA, Bottom, Bottom_And_Right, Left, Left_And_Right, Overstruck, Right,
        Top, Top_And_Bottom, Top_And_Bottom_And_Right, Top_And_Left,
        Top_And_Left_And_Right, Top_And_Right, Visual_Order_Left
    )

    RS_ENUM_CLASS(Indic_Syllabic_Category, int, 0,
        Other, Avagraha, Bindu, Brahmi_Joining_Number, Cantillation_Mark,
        Consonant, Consonant_Dead, Consonant_Final, Consonant_Head_Letter,
        Consonant_Killer, Consonant_Medial, Consonant_Placeholder,
        Consonant_Preceding_Repha, Consonant_Prefixed, Consonant_Subjoined,
        Consonant_Succeeding_Repha, Consonant_With_Stacker, Gemination_Mark,
        Invisible_Stacker, Joiner, Modifying_Letter, Non_Joiner, Nukta,
        Number, Number_Joiner, Pure_Killer, Register_Shifter,
        Syllable_Modifier, Tone_Letter, Tone_Mark, Virama, Visarga, Vowel,
        Vowel_Dependent, Vowel_Independent
    )

    RS_ENUM_CLASS(Joining_Group, int, 0,
        No_Joining_Group, Ain, Alaph, Alef, Beh, Beth, Burushaski_Yeh_Barree,
        Dal, Dalath_Rish, E, Farsi_Yeh, Fe, Feh, Final_Semkath, Gaf, Gamal,
        Hah, He, Heh, Heh_Goal, Heth, Kaf, Kaph, Khaph, Knotted_Heh, Lam,
        Lamadh, Manichaean_Aleph, Manichaean_Ayin, Manichaean_Beth,
        Manichaean_Daleth, Manichaean_Dhamedh, Manichaean_Gimel,
        Manichaean_Heth, Manichaean_Kaph, Manichaean_Lamedh, Manichaean_Mem,
        Manichaean_Nun, Manichaean_Pe, Manichaean_Samekh, Manichaean_Teth,
        Manichaean_Thamedh, Manichaean_Waw, Manichaean_Yodh, Manichaean_Zayin,
        Manichaean_Sadhe, Manichaean_Qoph, Manichaean_Resh, Manichaean_Taw,
        Manichaean_One, Manichaean_Five, Manichaean_Ten, Manichaean_Twenty,
        Manichaean_Hundred, Meem, Mim, Noon, Nun, Nya, Pe, Qaf, Qaph, Reh,
        Reversed_Pe, Rohingya_Yeh, Sad, Sadhe, Seen, Semkath, Shin,
        Straight_Waw, Swash_Kaf, Syriac_Waw, Tah, Taw, Teh_Marbuta,
        Teh_Marbuta_Goal, Teth, Waw, Yeh, Yeh_Barree, Yeh_With_Tail, Yudh,
        Yudh_He, Zain, Zhain
    )

    RS_ENUM_CLASS(Joining_Type, int, 0,
        Default, Dual_Joining, Join_Causing, Left_Joining, Non_Joining,
        Right_Joining, Transparent
    )

    RS_ENUM_CLASS(Line_Break, int, 0,
        XX, AI, AL, B2, BA, BB, BK, CB, CJ, CL, CM, CP, CR, EX, GL, H2, H3,
        HL, HY, ID, IN_, IS, JL, JT, JV, LF, NL, NS, NU, OP, PO, PR, QU, RI,
        SA, SG, SP, SY, WJ, ZW
    )

    RS_ENUM_CLASS(Numeric_Type, int, 0,
        None, Decimal, Digit, Numeric
    )

    RS_ENUM_CLASS(Sentence_Break, int, 0,
        Other, ATerm, Close, CR, EOT, Extend, Format, LF, Lower, Numeric,
        OLetter, SContinue, Sep, SOT, Sp, STerm, Upper
    )

    RS_ENUM_CLASS(Word_Break, int, 0,
        Other, ALetter, CR, Double_Quote, EOT, Extend, ExtendNumLet, Format,
        Hebrew_Letter, Katakana, LF, MidLetter, MidNum, MidNumLet, Newline,
        Numeric, Regional_Indicator, Single_Quote, SOT
    )

}
