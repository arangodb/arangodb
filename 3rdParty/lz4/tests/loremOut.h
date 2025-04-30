/*
    loremOut.h - lorem ipsum generator to stdout
    Copyright (C) Yann Collet 2024

    GPL v2 License

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

    You can contact the author at :
   - LZ4 source repository : https://github.com/lz4/lz4
   - Public forum : https://groups.google.com/forum/#!forum/lz4c
*/


/* LOREM_genOut():
 * Generate @size bytes of compressible data using lorem ipsum generator into stdout.
 */
void LOREM_genOut(unsigned long long size, unsigned seed);
