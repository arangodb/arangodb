////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "prime-numbers.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief pre-calculated prime numbers
////////////////////////////////////////////////////////////////////////////////

static uint64_t const Primes[251] = {
    7ULL,          11ULL,         13ULL,         17ULL,         19ULL,
    23ULL,         29ULL,         31ULL,         37ULL,         41ULL,
    47ULL,         53ULL,         59ULL,         67ULL,         73ULL,
    79ULL,         89ULL,         97ULL,         107ULL,        127ULL,
    137ULL,        149ULL,        163ULL,        179ULL,        193ULL,
    211ULL,        227ULL,        251ULL,        271ULL,        293ULL,
    317ULL,        347ULL,        373ULL,        401ULL,        431ULL,
    467ULL,        503ULL,        541ULL,        587ULL,        641ULL,
    691ULL,        751ULL,        809ULL,        877ULL,        947ULL,
    1019ULL,       1097ULL,       1181ULL,       1277ULL,       1381ULL,
    1487ULL,       1601ULL,       1733ULL,       1867ULL,       2011ULL,
    2179ULL,       2347ULL,       2531ULL,       2729ULL,       2939ULL,
    3167ULL,       3413ULL,       3677ULL,       3967ULL,       4273ULL,
    4603ULL,       4957ULL,       5347ULL,       5779ULL,       6229ULL,
    6709ULL,       7229ULL,       7789ULL,       8389ULL,       9041ULL,
    9739ULL,       10499ULL,      11311ULL,      12197ULL,      13147ULL,
    14159ULL,      15259ULL,      16433ULL,      17707ULL,      19069ULL,
    20543ULL,      22123ULL,      23827ULL,      25667ULL,      27647ULL,
    29789ULL,      32083ULL,      34583ULL,      37243ULL,      40111ULL,
    43201ULL,      46549ULL,      50129ULL,      53987ULL,      58147ULL,
    62627ULL,      67447ULL,      72643ULL,      78233ULL,      84263ULL,
    90749ULL,      97729ULL,      105251ULL,     113357ULL,     122081ULL,
    131477ULL,     141601ULL,     152501ULL,     164231ULL,     176887ULL,
    190507ULL,     205171ULL,     220973ULL,     237971ULL,     256279ULL,
    275999ULL,     297233ULL,     320101ULL,     344749ULL,     371281ULL,
    399851ULL,     430649ULL,     463781ULL,     499459ULL,     537883ULL,
    579259ULL,     623839ULL,     671831ULL,     723529ULL,     779189ULL,
    839131ULL,     903691ULL,     973213ULL,     1048123ULL,    1128761ULL,
    1215623ULL,    1309163ULL,    1409869ULL,    1518329ULL,    1635133ULL,
    1760917ULL,    1896407ULL,    2042297ULL,    2199401ULL,    2368589ULL,
    2550791ULL,    2747021ULL,    2958331ULL,    3185899ULL,    3431009ULL,
    3694937ULL,    3979163ULL,    4285313ULL,    4614959ULL,    4969961ULL,
    5352271ULL,    5763991ULL,    6207389ULL,    6684907ULL,    7199147ULL,
    7752929ULL,    8349311ULL,    8991599ULL,    9683263ULL,    10428137ULL,
    11230309ULL,   12094183ULL,   13024507ULL,   14026393ULL,   15105359ULL,
    16267313ULL,   17518661ULL,   18866291ULL,   20317559ULL,   21880459ULL,
    23563571ULL,   25376179ULL,   27328211ULL,   29430391ULL,   31694281ULL,
    34132321ULL,   36757921ULL,   39585457ULL,   42630499ULL,   45909769ULL,
    49441289ULL,   53244481ULL,   57340211ULL,   61750999ULL,   66501077ULL,
    71616547ULL,   77125553ULL,   83058289ULL,   89447429ULL,   96328003ULL,
    103737857ULL,  111717757ULL,  120311453ULL,  129566201ULL,  139532831ULL,
    150266159ULL,  161825107ULL,  174273193ULL,  187678831ULL,  202115701ULL,
    217663079ULL,  234406397ULL,  252437677ULL,  271855963ULL,  292767983ULL,
    315288607ULL,  339541597ULL,  365660189ULL,  393787907ULL,  424079291ULL,
    456700789ULL,  491831621ULL,  529664827ULL,  570408281ULL,  614285843ULL,
    661538611ULL,  712426213ULL,  767228233ULL,  826245839ULL,  889803241ULL,
    958249679ULL,  1031961197ULL, 1111342867ULL, 1196830801ULL, 1288894709ULL,
    1388040461ULL, 1494812807ULL, 1609798417ULL, 1733629067ULL, 1866985157ULL,
    2010599411ULL, 2165260961ULL, 2331819499ULL, 2511190229ULL, 2704358747ULL,
    2912386343ULL, 3136416067ULL, 3377678861ULL, 3637500323ULL, 3917308049ULL,
    4218639443ULL};

static_assert(sizeof(Primes) / sizeof(Primes[0]) == 251,
              "invalid prime table size");

////////////////////////////////////////////////////////////////////////////////
/// @brief return a prime number not lower than value
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_NearPrime(uint64_t value) {
  for (unsigned int i = 0; i < sizeof(Primes) / sizeof(Primes[0]); ++i) {
    if (Primes[i] >= value) {
      return Primes[i];
    }
  }
  return value;
}
