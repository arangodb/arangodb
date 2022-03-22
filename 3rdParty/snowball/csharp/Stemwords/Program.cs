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
    using System.IO;
    using System.Reflection;
    using System.Linq;
    using System.Text;

    /// <summary>
    ///   Snowball's Stemmer program.
    /// </summary>
    /// 
    public static class Program
    {

        private static void usage()
        {
            Console.WriteLine("Usage: stemwords.exe -l <language> -i <input file> [-o <output file>]");
        }

        /// <summary>
        ///   Main program entrypoint.
        /// </summary>
        /// 
        public static void Main(String[] args)
        {
            string language = null;
            string inputName = null;
            string outputName = null;

            for (int i = 0; i < args.Length; i++)
            {
                if (args[i] == "-l")
                    language = args[i + 1];
                else if (args[i] == "-i")
                    inputName = args[i + 1];
                if (args[i] == "-o")
                    outputName = args[i + 1];
            }

            if (language == null || inputName == null)
            {
                usage();
                return;
            }



            Stemmer stemmer =
                typeof(Stemmer).Assembly.GetTypes()
                    .Where(t => t.IsSubclassOf(typeof(Stemmer)) && !t.IsAbstract)
                    .Where(t => match(t.Name, language))
                    .Select(t => (Stemmer)Activator.CreateInstance(t)).FirstOrDefault();

            if (stemmer == null)
            {
                Console.WriteLine("Language not found.");
                return;
            }

            Console.WriteLine("Using " + stemmer.GetType());

            TextWriter output = System.Console.Out;

            if (outputName != null)
                output = new StreamWriter(outputName);


            foreach (var line in File.ReadAllLines(inputName))
            {
                var o = stemmer.Stem(line);
                output.WriteLine(o);
            }

            output.Flush();
        }

        private static bool match(string stemmerName, string language)
        {
            string expectedName = language.Replace("_", "") + "Stemmer";

            return stemmerName.StartsWith(expectedName,
                StringComparison.CurrentCultureIgnoreCase);
        }
    }
}
