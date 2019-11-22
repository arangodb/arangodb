// Copyright (c) 2001, Dr Martin Porter
// Copyright (c) 2002, Richard Boulton
// Copyright (c) 2015, Cesar Souza
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
    using System.Text;

    /// <summary>
    ///   Snowball's among construction.
    /// </summary>
    /// 
    public sealed class Among
    {
        /// <summary>
        ///   Search string.
        /// </summary>
        /// 
        public string SearchString { get; private set; }

        /// <summary>
        ///   Index to longest matching substring.
        /// </summary>
        /// 
        public int MatchIndex { get; private set; }

        /// <summary>
        ///   Result of the lookup.
        /// </summary>
        /// 
        public int Result { get; private set; }

        /// <summary>
        ///   Action to be invoked.
        /// </summary>
        /// 
        public Func<bool> Action { get; private set; }

        /// <summary>
        ///   Initializes a new instance of the <see cref="Among"/> class.
        /// </summary>
        /// 
        /// <param name="str">The search string.</param>
        /// <param name="index">The index to the longest matching substring.</param>
        /// <param name="result">The result of the lookup.</param>
        /// 
        public Among(String str, int index, int result)
            : this(str, index, result, null)
        {
        }

        /// <summary>
        ///   Initializes a new instance of the <see cref="Among"/> class.
        /// </summary>
        /// 
        /// <param name="str">The search string.</param>
        /// <param name="index">The index to the longest matching substring.</param>
        /// <param name="result">The result of the lookup.</param>
        /// <param name="action">The action to be performed, if any.</param>
        /// 
        public Among(String str, int index, int result, Func<bool> action)
        {
            this.SearchString = str;
            this.MatchIndex = index;
            this.Result = result;
            this.Action = action;
        }

        /// <summary>
        ///   Returns a <see cref="System.String" /> that represents this instance.
        /// </summary>
        /// 
        /// <returns>
        ///   A <see cref="System.String" /> that represents this instance.
        /// </returns>
        /// 
        public override string ToString()
        {
            return SearchString;
        }

    }
}
