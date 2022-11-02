// Copyright (c) 2001, Dr Martin Porter
// Copyright (c) 2002, Richard Boulton
// Copyright (c) 2015, Cesar Souza
// Copyright (c) 2018, Olly Betts
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//     * this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//     * notice, this list of conditions and the following disclaimer in the
//     * documentation and/or other materials provided with the distribution.
//     * Neither the name of the copyright holders nor the names of its contributors
//     * may be used to endorse or promote products derived from this software
//     * without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

namespace Snowball
{
    using System;
    using System.Linq;
    using System.Text;

    /// <summary>
    ///   Class holding current state.
    /// </summary>
    /// 
    public class Env
    {
        /// <summary>
        ///   Initializes a new instance of the <see cref="Env"/> class.
        /// </summary>
        /// 
        protected Env()
        {
        }

        /// <summary>
        ///   Gets the current string.
        /// </summary>
        /// 
        protected StringBuilder current;

        /// <summary>
        ///   Current cursor position.
        /// </summary>
        /// 
        protected int cursor;

        /// <summary>
        ///   Forward limit for inspecting the buffer.
        /// </summary>
        /// 
        protected int limit;

        /// <summary>
        ///   Backward limit for inspecting the buffer.
        /// </summary>
        /// 
        protected int limit_backward;

        /// <summary>
        ///   Starting bracket position.
        /// </summary>
        /// 
        protected int bra;

        /// <summary>
        ///   Ending bracket position.
        /// </summary>
        /// 
        protected int ket;

        /// <summary>
        ///   Copy another Env object.
        /// </summary>
        /// 
        public Env(Env other)
        {
            copy_from(other);
        }

        /// <summary>
        ///   Copy another Env object.
        /// </summary>
        /// 
        protected void copy_from(Env other)
        {
            current          = other.current;
            cursor           = other.cursor;
            limit            = other.limit;
            limit_backward   = other.limit_backward;
            bra              = other.bra;
            ket              = other.ket;
        }
    }


    /// <summary>
    ///   Base class for Snowball's stemmer algorithms.
    /// </summary>
    /// 
    public abstract class Stemmer : Env
    {
        /// <summary>
        ///   Initializes a new instance of the <see cref="Stemmer"/> class.
        /// </summary>
        /// 
        protected Stemmer()
        {
            current = new StringBuilder();
            setBufferContents("");
        }


        /// <summary>
        ///   Calls the stemmer to process the next word.
        /// </summary>
        /// 
        protected abstract bool stem();


        /// <summary>
        ///   Stems the buffer's contents.
        /// </summary>
        /// 
        public bool Stem()
        {
            return this.stem();
        }

        /// <summary>
        ///   Stems a given word.
        /// </summary>
        /// 
        /// <param name="word">The word to be stemmed.</param>
        /// 
        /// <returns>The stemmed word.</returns>
        /// 
        public string Stem(string word)
        {
            setBufferContents(word);
            this.stem();
            return current.ToString();
        }


        /// <summary>
        ///   Gets the current processing buffer.
        /// </summary>
        /// 
        public StringBuilder Buffer
        {
            get { return current; }
        }

        /// <summary>
        ///   Gets or sets the current word to be stemmed
        ///   or the stemmed word, if the stemmer has been
        ///   processed.
        /// </summary>
        /// 
        public string Current
        {
            get { return current.ToString(); }
            set { setBufferContents(value); }
        }

        private void setBufferContents(string value)
        {
            current.Clear();
            current.Insert(0, value);

            cursor = 0;
            limit = current.Length;
            limit_backward = 0;
            bra = cursor;
            ket = limit;
        }


        /// <summary>
        ///   Determines whether the current character is 
        ///   inside a given group of characters <c>s</c>.
        /// </summary>
        protected int in_grouping(string s, int min, int max, bool repeat)
        {
            do
            {
                if (cursor >= limit)
                    return -1;

                char ch = current[cursor];
                if (ch > max || ch < min)
                    return 1;

                if (!s.Contains(ch))
                    return 1;

                cursor++;
            }
            while (repeat);

            return 0;
        }

        /// <summary>
        ///   Determines whether the current character is 
        ///   inside a given group of characters <c>s</c>.
        /// </summary>
        protected int in_grouping_b(string s, int min, int max, bool repeat)
        {
            do
            {
                if (cursor <= limit_backward)
                    return -1;

                char ch = current[cursor - 1];
                if (ch > max || ch < min)
                    return 1;

                if (!s.Contains(ch))
                    return 1;

                cursor--;
            } while (repeat);

            return 0;
        }

        /// <summary>
        ///   Determines whether the current character is 
        ///   outside a given group of characters <c>s</c>.
        /// </summary>
        protected int out_grouping(string s, int min, int max, bool repeat)
        {
            do
            {
                if (cursor >= limit)
                    return -1;

                char ch = current[cursor];
                if (ch > max || ch < min)
                {
                    cursor++;
                    continue;
                }

                if (!s.Contains(ch))
                {
                    cursor++;
                    continue;
                }

                return 1;

            } while (repeat);

            return 0;
        }

        /// <summary>
        ///   Determines whether the current character is 
        ///   outside a given group of characters <c>s</c>.
        /// </summary>
        protected int out_grouping_b(string s, int min, int max, bool repeat)
        {
            do
            {
                if (cursor <= limit_backward)
                    return -1;

                char ch = current[cursor - 1];
                if (ch > max || ch < min)
                {
                    cursor--;
                    continue;
                }

                if (!s.Contains(ch))
                {
                    cursor--;
                    continue;
                }

                return 1;
            }
            while (repeat);

            return 0;
        }


        /// <summary>
        ///   Determines if the current buffer contains the
        ///   string s, starting from the current position and
        ///   going forward.
        /// </summary>
        protected bool eq_s(String s)
        {
            if (limit - cursor < s.Length)
                return false;

            for (int i = 0; i != s.Length; i++)
            {
                if (current[cursor + i] != s[i])
                    return false;
            }

            cursor += s.Length;
            return true;
        }

        /// <summary>
        ///   Determines if the current buffer contains the
        ///   string s, starting from the current position and
        ///   going backwards.
        /// </summary>
        protected bool eq_s_b(String s)
        {
            if (cursor - limit_backward < s.Length)
                return false;

            for (int i = 0; i != s.Length; i++)
            {
                if (current[cursor - s.Length + i] != s[i])
                    return false;
            }

            cursor -= s.Length;
            return true;
        }

        /// <summary>
        ///   Determines if the current buffer contains the
        ///   string s, starting from the current position and
        ///   going backwards.
        /// </summary>
        protected bool eq_s_b(StringBuilder s)
        {
            if (cursor - limit_backward < s.Length)
                return false;

            for (int i = 0; i != s.Length; i++)
            {
                if (current[cursor - s.Length + i] != s[i])
                    return false;
            }

            cursor -= s.Length;
            return true;
        }


        /// <summary>
        ///   Searches if the current buffer matches against one of the 
        ///   amongs, starting from the current cursor position and going
        ///   forward.
        /// </summary>
        /// 
        protected int find_among(Among[] v)
        {
            int i = 0;
            int j = v.Length;

            int c = cursor;
            int l = limit;

            int common_i = 0;
            int common_j = 0;

            bool first_key_inspected = false;

            while (true)
            {
                int k = i + ((j - i) >> 1);
                int diff = 0;
                int common = common_i < common_j ? common_i : common_j; // smaller

                Among w = v[k];

                for (int i2 = common; i2 < w.SearchString.Length; i2++)
                {
                    if (c + common == l)
                    {
                        diff = -1;
                        break;
                    }

                    diff = current[c + common] - w.SearchString[i2];

                    if (diff != 0)
                        break;

                    common++;
                }

                if (diff < 0)
                {
                    j = k;
                    common_j = common;
                }
                else
                {
                    i = k;
                    common_i = common;
                }

                if (j - i <= 1)
                {
                    if (i > 0)
                        break; // v->s has been inspected

                    if (j == i)
                        break; // only one item in v

                    // - but now we need to go round once more to get
                    // v->s inspected. This looks messy, but is actually
                    // the optimal approach.

                    if (first_key_inspected)
                        break;

                    first_key_inspected = true;
                }
            }

            while (true)
            {
                Among w = v[i];

                if (common_i >= w.SearchString.Length)
                {
                    cursor = c + w.SearchString.Length;

                    if (w.Action == null)
                        return w.Result;

                    bool res = w.Action();
                    cursor = c + w.SearchString.Length;

                    if (res)
                        return w.Result;
                }

                i = w.MatchIndex;

                if (i < 0)
                    return 0;
            }
        }

        /// <summary>
        ///   Searches if the current buffer matches against one of the 
        ///   amongs, starting from the current cursor position and going
        ///   backwards.
        /// </summary>
        /// 
        protected int find_among_b(Among[] v)
        {
            int i = 0;
            int j = v.Length;

            int c = cursor;
            int lb = limit_backward;

            int common_i = 0;
            int common_j = 0;

            bool first_key_inspected = false;

            while (true)
            {
                int k = i + ((j - i) >> 1);
                int diff = 0;
                int common = common_i < common_j ? common_i : common_j;
                Among w = v[k];

                for (int i2 = w.SearchString.Length - 1 - common; i2 >= 0; i2--)
                {
                    if (c - common == lb)
                    {
                        diff = -1;
                        break;
                    }

                    diff = current[c - 1 - common] - w.SearchString[i2];

                    if (diff != 0)
                        break;

                    common++;
                }

                if (diff < 0)
                {
                    j = k;
                    common_j = common;
                }
                else
                {
                    i = k;
                    common_i = common;
                }

                if (j - i <= 1)
                {
                    if (i > 0)
                        break;

                    if (j == i)
                        break;

                    if (first_key_inspected)
                        break;

                    first_key_inspected = true;
                }
            }

            while (true)
            {
                Among w = v[i];

                if (common_i >= w.SearchString.Length)
                {
                    cursor = c - w.SearchString.Length;

                    if (w.Action == null)
                        return w.Result;

                    bool res = w.Action();
                    cursor = c - w.SearchString.Length;

                    if (res)
                        return w.Result;
                }

                i = w.MatchIndex;

                if (i < 0)
                    return 0;
            }
        }

        /// <summary>
        ///   Replaces the characters between <c>c_bra</c>
        ///   and <c>c_ket</c> by the characters in s.
        /// </summary>
        ///   
        protected int replace_s(int c_bra, int c_ket, String s)
        {
            int adjustment = s.Length - (c_ket - c_bra);
            Replace(current, c_bra, c_ket, s);
            limit += adjustment;
            if (cursor >= c_ket)
                cursor += adjustment;
            else if (cursor > c_bra)
                cursor = c_bra;
            return adjustment;
        }

        /// <summary>
        ///   Checks if a slicing can be done.
        /// </summary>
        protected void slice_check()
        {
            if (bra < 0 || bra > ket || ket > limit || limit > current.Length)
            {
                System.Diagnostics.Trace.WriteLine("faulty slice operation");
            }
        }

        /// <summary>
        ///   Replaces the contents of the bracket with the string s.
        /// </summary>
        /// 
        /// <param name="s">The s.</param>
        protected void slice_from(String s)
        {
            slice_check();
            replace_s(bra, ket, s);
        }

        /// <summary>
        ///   Removes the current bracket contents.
        /// </summary>
        /// 
        protected void slice_del()
        {
            slice_from("");
        }

        /// <summary>
        ///   Replaces the contents of the bracket with the string s.
        /// </summary>
        /// 
        protected void insert(int c_bra, int c_ket, String s)
        {
            int adjustment = replace_s(c_bra, c_ket, s);
            if (c_bra <= bra) bra += adjustment;
            if (c_bra <= ket) ket += adjustment;
        }

        /// <summary>
        ///   Replaces the contents of the bracket with the string s.
        /// </summary>
        /// 
        protected void insert(int c_bra, int c_ket, StringBuilder s)
        {
            int adjustment = replace_s(c_bra, c_ket, s.ToString());
            if (c_bra <= bra) bra += adjustment;
            if (c_bra <= ket) ket += adjustment;
        }

        /// <summary>
        ///   Replaces the contents of the bracket with the string s.
        /// </summary>
        /// 
        protected void slice_to(StringBuilder s)
        {
            slice_check();
            Replace(s, 0, s.Length, current.ToString(bra, ket - bra));
        }

        /// <summary>
        ///   Replaces the contents of the bracket with the string s.
        /// </summary>
        /// 
        protected void assign_to(StringBuilder s)
        {
            Replace(s, 0, s.Length, current.ToString(0, limit));
        }



        /// <summary>
        ///   Replaces a specific region of the buffer with another text.
        /// </summary>
        public static StringBuilder Replace(StringBuilder sb, int index, int length, string text)
        {
            sb.Remove(index, length - index);
            sb.Insert(index, text);
            return sb;
        }

    }
}
