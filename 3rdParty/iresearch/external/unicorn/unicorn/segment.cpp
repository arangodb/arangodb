#include "unicorn/segment.hpp"

using namespace std::literals;

namespace RS::Unicorn {

    namespace {

        template <typename P>
        inline P prop(const std::deque<P>& buf, size_t i) {
            return i < buf.size() ? buf[i] : P::EOT;
        }

    }

    namespace UnicornDetail {

        // Unicode Standard Annex #29: Unicode Text Segmentation
        // http://www.unicode.org/reports/tr29

        size_t find_grapheme_break(const std::deque<Grapheme_Cluster_Break>& buf, bool /*eof*/) {
            using P = Grapheme_Cluster_Break;
            if (buf.empty())
                return 0;
            size_t size = buf.size();
            for (size_t i = 1; i < size; ++i) {
                // Break at the start and end of text.
                // GB1. sot ÷
                // GB2. ÷ eot
                // Do not break between a CR and LF. Otherwise, break before and after controls.
                // GB3. CR × LF
                // GB4. (Control | CR | LF) ÷
                // GB5. ÷ (Control | CR | LF)
                P prev(prop(buf, i - 1));
                P next(prop(buf, i));
                if (prev == P::CR && next == P::LF)
                    continue;
                if (prev == P::Control || prev == P::CR || prev == P::LF
                        || next == P::Control || next == P::CR || next == P::LF)
                    return i;
                // Do not break Hangul syllable sequences.
                // GB6. L × (L | V | LV | LVT)
                if (prev == P::L
                        && (next == P::L || next == P::V || next == P::LV || next == P::LVT))
                    continue;
                // GB7. (LV | V) × (V | T)
                if ((prev == P::LV || prev == P::V) && (next == P::V || next == P::T))
                    continue;
                // GB8. (LVT | T) × T
                if ((prev == P::LVT || prev == P::T) && next == P::T)
                    continue;
                // Do not break between regional indicator symbols.
                // GB8a. Regional_Indicator × Regional_Indicator
                if (prev == P::Regional_Indicator && next == P::Regional_Indicator)
                    continue;
                // Do not break before extending characters.
                // GB9. × Extend
                // Do not break before SpacingMarks, or after Prepend characters.
                // GB9a. × SpacingMark
                // GB9b. Prepend ×
                if (prev == P::Prepend || next == P::Extend || next == P::SpacingMark)
                    continue;
                // Otherwise, break everywhere.
                // GB10. Any ÷ Any
                return i;
            }
            return 0;
        }

        size_t find_word_break(const std::deque<Word_Break>& buf, bool eof) {
            using P = Word_Break;
            if (buf.empty())
                return 0;
            size_t size = buf.size();
            for (size_t i = 1; i < size; ++i) {
                // Break at the start and end of text.
                // WB1. sot ÷
                // WB2. ÷ eot
                // Do not break within CRLF.
                // WB3. CR × LF
                P prev(prop(buf, i - 1));
                P next(prop(buf, i));
                if (prev == P::CR && next == P::LF)
                    continue;
                // Otherwise break before and after Newlines (including CR and LF)
                // WB3a. (Newline | CR | LF) ÷
                // WB3b. ÷ (Newline | CR | LF)
                if (prev == P::CR || prev == P::LF || prev == P::Newline
                        || next == P::CR || next == P::LF || next == P::Newline)
                    return i;
                // Ignore Format and Extend characters, except when they
                // appear at the beginning of a region of text.
                // WB4. X (Extend | Format)* → X
                if (next == P::Extend || next == P::Format)
                    continue;
                size_t j = i;
                do prev = prop(buf, --j);
                    while (prev == P::Extend || prev == P::Format);
                P prev2;
                do prev2 = prop(buf, --j);
                    while (prev2 == P::Extend || prev2 == P::Format);
                j = i;
                P next2;
                do next2 = prop(buf, ++j);
                    while (next2 == P::Extend || next2 == P::Format);
                // Do not break between most letters.
                // WB5. (ALetter | Hebrew_Letter) × (ALetter | Hebrew_Letter)
                if ((prev == P::ALetter || prev == P::Hebrew_Letter)
                        && (next == P::ALetter || next == P::Hebrew_Letter))
                    continue;
                // Do not break letters across certain punctuation.
                // WB6. (ALetter | Hebrew_Letter) × (MidLetter | MidNumLet | Single_Quote) (ALetter | Hebrew_Letter)
                if ((prev == P::ALetter || prev == P::Hebrew_Letter)
                        && (next == P::MidLetter || next == P::MidNumLet || next == P::Single_Quote)) {
                    if (next2 == P::EOT && ! eof)
                        return 0;
                    if (next2 == P::ALetter || next2 == P::Hebrew_Letter)
                        continue;
                }
                // WB7. (ALetter | Hebrew_Letter) (MidLetter | MidNumLet | Single_Quote) × (ALetter | Hebrew_Letter)
                if ((prev2 == P::ALetter || prev2 == P::Hebrew_Letter)
                        && (prev == P::MidLetter || prev == P::MidNumLet || prev == P::Single_Quote)
                        && (next == P::ALetter || next == P::Hebrew_Letter))
                    continue;
                // WB7a. Hebrew_Letter × Single_Quote
                if (prev == P::Hebrew_Letter && next == P::Single_Quote)
                    continue;
                // WB7b. Hebrew_Letter × Double_Quote Hebrew_Letter
                if (prev == P::Hebrew_Letter && next == P::Double_Quote) {
                    if (next2 == P::EOT && ! eof)
                        return 0;
                    if (next2 == P::Hebrew_Letter)
                        continue;
                }
                // WB7c. Hebrew_Letter Double_Quote × Hebrew_Letter
                if (prev2 == P::Hebrew_Letter && prev == P::Double_Quote && next == P::Hebrew_Letter)
                    continue;
                // Do not break within sequences of digits, or digits adjacent to letters.
                // WB8. Numeric × Numeric
                // WB9. (ALetter | Hebrew_Letter) × Numeric
                if ((prev == P::ALetter || prev == P::Hebrew_Letter || prev == P::Numeric)
                        && next == P::Numeric)
                    continue;
                // WB10. Numeric × (ALetter | Hebrew_Letter)
                if (prev == P::Numeric && (next == P::ALetter || next == P::Hebrew_Letter))
                    continue;
                // Do not break within sequences, such as “3.2” or “3,456.789”.
                // WB11. Numeric (MidNum | MidNumLet | Single_Quote) × Numeric
                if (prev2 == P::Numeric
                        && (prev == P::MidNum || prev == P::MidNumLet || prev == P::Single_Quote)
                        && next == P::Numeric)
                    continue;
                // WB12. Numeric × (MidNum | MidNumLet | Single_Quote) Numeric
                if (prev == P::Numeric
                        && (next == P::MidNum || next == P::MidNumLet || next == P::Single_Quote)) {
                    if (next2 == P::EOT && ! eof)
                        return 0;
                    if (next2 == P::Numeric)
                        continue;
                }
                // Do not break between Katakana.
                // WB13. Katakana × Katakana
                if (prev == P::Katakana && next == P::Katakana)
                    continue;
                // Do not break from extenders.
                // WB13a. (ALetter | Hebrew_Letter | Numeric | Katakana | ExtendNumLet) × ExtendNumLet
                if ((prev == P::ALetter || prev == P::ExtendNumLet || prev == P::Hebrew_Letter
                            || prev == P::Katakana || prev == P::Numeric)
                        && next == P::ExtendNumLet)
                    continue;
                // WB13b. ExtendNumLet × (ALetter | Hebrew_Letter | Numeric | Katakana)
                if (prev == P::ExtendNumLet
                        && (next == P::ALetter || next == P::Hebrew_Letter || next == P::Katakana
                            || next == P::Numeric))
                    continue;
                // Do not break between regional indicator symbols.
                // WB13c. Regional_Indicator × Regional_Indicator
                if (prev == P::Regional_Indicator && next == P::Regional_Indicator)
                    continue;
                // Otherwise, break everywhere (including around ideographs).
                // WB14. Any ÷ Any
                return i;
            }
            return 0;
        }

        size_t find_sentence_break(const std::deque<Sentence_Break>& buf, bool eof) {
            using P = Sentence_Break;
            if (buf.empty())
                return 0;
            size_t size = buf.size();
            for (size_t i = 1; i < size; ++i) {
                // Break at the start and end of text.
                // SB1. sot ÷
                // SB2. ÷ eot
                // Do not break within CRLF.
                // SB3. CR × LF
                P prev(prop(buf, i - 1));
                P next(prop(buf, i));
                if (prev == P::CR && next == P::LF)
                    continue;
                // Break after paragraph separators.
                // SB4. Sep | CR | LF ÷
                if (prev == P::CR || prev == P::LF || prev == P::Sep)
                    return i;
                // Ignore Format and Extend characters, except when they
                // appear at the beginning of a region of text.
                // SB5. X (Extend | Format)* → X
                if (next == P::Extend || next == P::Format)
                    continue;
                size_t j = i;
                do prev = prop(buf, --j);
                    while (prev == P::Extend || prev == P::Format);
                P prev2;
                do prev2 = prop(buf, --j);
                    while (prev2 == P::Extend || prev2 == P::Format);
                // Do not break after ambiguous terminators like period, if
                // they are immediately followed by a number or lowercase
                // letter, if they are between uppercase letters, if the first
                // following letter (optionally after certain punctuation) is
                // lowercase, or if they are followed by “continuation”
                // punctuation such as comma, colon, or semicolon.
                // SB6. ATerm × Numeric
                if (prev == P::ATerm && next == P::Numeric)
                    continue;
                // SB7. (Upper | Lower) ATerm × Upper
                if ((prev2 == P::Upper || prev2 == P::Lower) && prev == P::ATerm && next == P::Upper)
                    continue;
                // SB8. ATerm Close* Sp* × (¬(OLetter | Upper | Lower | Sep | CR | LF | STerm | ATerm))* Lower
                j = i;
                P pre_cs;
                do pre_cs = prop(buf, --j);
                    while (pre_cs == P::Extend || pre_cs == P::Format || pre_cs == P::Sp);
                while (pre_cs == P::Extend || pre_cs == P::Format || pre_cs == P::Close)
                    pre_cs = prop(buf, --j);
                if (pre_cs == P::ATerm) {
                    j = i;
                    P post(prop(buf, j));
                    while (post == P::Extend || post == P::Format
                            || (post != P::ATerm && post != P::EOT && post != P::CR && post != P::LF && post != P::Lower
                                && post != P::OLetter && post != P::Sep && post != P::STerm && post != P::Upper))
                        post = prop(buf, ++j);
                    if (post == P::EOT && ! eof)
                        return 0;
                    if (post == P::Lower)
                        continue;
                }
                // SB8a. (STerm | ATerm) Close* Sp* × (SContinue | STerm | ATerm)
                if ((pre_cs == P::ATerm || pre_cs == P::STerm)
                        && (next == P::ATerm || next == P::SContinue || next == P::STerm))
                    continue;
                // Break after sentence terminators, but include closing
                // punctuation, trailing spaces, and a paragraph separator (if
                // present).
                // SB9. (STerm | ATerm) Close* × (Close | Sp | Sep | CR | LF)
                j = i;
                P pre_c;
                do pre_c = prop(buf, --j);
                    while (pre_c == P::Extend || pre_c == P::Format || pre_c == P::Close);
                if ((pre_c == P::ATerm || pre_c == P::STerm)
                        && (next == P::Close || next == P::CR || next == P::LF || next == P::Sep || next == P::Sp))
                    continue;
                // SB10. (STerm | ATerm) Close* Sp* × (Sp | Sep | CR | LF)
                if ((pre_cs == P::ATerm || pre_cs == P::STerm)
                        && (next == P::CR || next == P::LF || next == P::Sep || next == P::Sp))
                    continue;
                // SB11. (STerm | ATerm) Close* Sp* (Sep | CR | LF)? ÷
                j = i;
                P pre_csx;
                do pre_csx = prop(buf, --j);
                    while (pre_csx == P::Extend || pre_csx == P::Format);
                if (pre_csx == P::CR || pre_csx == P::LF || pre_csx == P::Sep)
                    pre_csx = prop(buf, --j);
                while (pre_csx == P::Extend || pre_csx == P::Format || pre_csx == P::Sp)
                    pre_csx = prop(buf, --j);
                while (pre_csx == P::Extend || pre_csx == P::Format || pre_csx == P::Close)
                    pre_csx = prop(buf, --j);
                if (pre_csx == P::ATerm || pre_csx == P::STerm)
                    return i;
                // Otherwise, do not break.
                // SB12. Any × Any
            }
            return 0;
        }

    }

}
