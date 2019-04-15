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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "hashes.h"

/// @brief the FNV hash work horse
static inline uint64_t FnvWork(uint8_t value, uint64_t hash) {
  static uint64_t const MagicPrime = 0x00000100000001b3ULL;

  return (hash ^ value) * MagicPrime;
}

/// @brief computes a FNV hash for strings with a length
uint64_t TRI_FnvHashBlock(uint64_t hash, void const* buffer, size_t length) {
  uint8_t const* p = (uint8_t const*)buffer;
  uint8_t const* end = p + length;

  while (p < end) {
    hash = FnvWork(*p++, hash);
  }

  return hash;
}

/// @brief computes a FNV hash for memory blobs
uint64_t TRI_FnvHashPointer(void const* buffer, size_t length) {
  return TRI_FnvHashBlock(0xcbf29ce484222325ULL, buffer, length);
}

/// @brief computes a FNV hash for strings
uint64_t TRI_FnvHashString(char const* buffer) {
  uint64_t const nMagicPrime = 0x00000100000001b3ULL;
  uint64_t nHashVal = 0xcbf29ce484222325ULL;
  uint8_t const* pFirst = (uint8_t const*)buffer;

  while (*pFirst) {
    nHashVal ^= *pFirst++;
    nHashVal *= nMagicPrime;
  }

  return nHashVal;
}

/// @brief precomputed lookup values for crc32 8 bytes-at-a-time calculation
static const uint32_t Crc32Lookup[8][256] = {
    {0x00000000, 0xf26b8303, 0xe13b70f7, 0x1350f3f4, 0xc79a971f, 0x35f1141c,
     0x26a1e7e8, 0xd4ca64eb, 0x8ad958cf, 0x78b2dbcc, 0x6be22838, 0x9989ab3b,
     0x4d43cfd0, 0xbf284cd3, 0xac78bf27, 0x5e133c24, 0x105ec76f, 0xe235446c,
     0xf165b798, 0x030e349b, 0xd7c45070, 0x25afd373, 0x36ff2087, 0xc494a384,
     0x9a879fa0, 0x68ec1ca3, 0x7bbcef57, 0x89d76c54, 0x5d1d08bf, 0xaf768bbc,
     0xbc267848, 0x4e4dfb4b, 0x20bd8ede, 0xd2d60ddd, 0xc186fe29, 0x33ed7d2a,
     0xe72719c1, 0x154c9ac2, 0x061c6936, 0xf477ea35, 0xaa64d611, 0x580f5512,
     0x4b5fa6e6, 0xb93425e5, 0x6dfe410e, 0x9f95c20d, 0x8cc531f9, 0x7eaeb2fa,
     0x30e349b1, 0xc288cab2, 0xd1d83946, 0x23b3ba45, 0xf779deae, 0x05125dad,
     0x1642ae59, 0xe4292d5a, 0xba3a117e, 0x4851927d, 0x5b016189, 0xa96ae28a,
     0x7da08661, 0x8fcb0562, 0x9c9bf696, 0x6ef07595, 0x417b1dbc, 0xb3109ebf,
     0xa0406d4b, 0x522bee48, 0x86e18aa3, 0x748a09a0, 0x67dafa54, 0x95b17957,
     0xcba24573, 0x39c9c670, 0x2a993584, 0xd8f2b687, 0x0c38d26c, 0xfe53516f,
     0xed03a29b, 0x1f682198, 0x5125dad3, 0xa34e59d0, 0xb01eaa24, 0x42752927,
     0x96bf4dcc, 0x64d4cecf, 0x77843d3b, 0x85efbe38, 0xdbfc821c, 0x2997011f,
     0x3ac7f2eb, 0xc8ac71e8, 0x1c661503, 0xee0d9600, 0xfd5d65f4, 0x0f36e6f7,
     0x61c69362, 0x93ad1061, 0x80fde395, 0x72966096, 0xa65c047d, 0x5437877e,
     0x4767748a, 0xb50cf789, 0xeb1fcbad, 0x197448ae, 0x0a24bb5a, 0xf84f3859,
     0x2c855cb2, 0xdeeedfb1, 0xcdbe2c45, 0x3fd5af46, 0x7198540d, 0x83f3d70e,
     0x90a324fa, 0x62c8a7f9, 0xb602c312, 0x44694011, 0x5739b3e5, 0xa55230e6,
     0xfb410cc2, 0x092a8fc1, 0x1a7a7c35, 0xe811ff36, 0x3cdb9bdd, 0xceb018de,
     0xdde0eb2a, 0x2f8b6829, 0x82f63b78, 0x709db87b, 0x63cd4b8f, 0x91a6c88c,
     0x456cac67, 0xb7072f64, 0xa457dc90, 0x563c5f93, 0x082f63b7, 0xfa44e0b4,
     0xe9141340, 0x1b7f9043, 0xcfb5f4a8, 0x3dde77ab, 0x2e8e845f, 0xdce5075c,
     0x92a8fc17, 0x60c37f14, 0x73938ce0, 0x81f80fe3, 0x55326b08, 0xa759e80b,
     0xb4091bff, 0x466298fc, 0x1871a4d8, 0xea1a27db, 0xf94ad42f, 0x0b21572c,
     0xdfeb33c7, 0x2d80b0c4, 0x3ed04330, 0xccbbc033, 0xa24bb5a6, 0x502036a5,
     0x4370c551, 0xb11b4652, 0x65d122b9, 0x97baa1ba, 0x84ea524e, 0x7681d14d,
     0x2892ed69, 0xdaf96e6a, 0xc9a99d9e, 0x3bc21e9d, 0xef087a76, 0x1d63f975,
     0x0e330a81, 0xfc588982, 0xb21572c9, 0x407ef1ca, 0x532e023e, 0xa145813d,
     0x758fe5d6, 0x87e466d5, 0x94b49521, 0x66df1622, 0x38cc2a06, 0xcaa7a905,
     0xd9f75af1, 0x2b9cd9f2, 0xff56bd19, 0x0d3d3e1a, 0x1e6dcdee, 0xec064eed,
     0xc38d26c4, 0x31e6a5c7, 0x22b65633, 0xd0ddd530, 0x0417b1db, 0xf67c32d8,
     0xe52cc12c, 0x1747422f, 0x49547e0b, 0xbb3ffd08, 0xa86f0efc, 0x5a048dff,
     0x8ecee914, 0x7ca56a17, 0x6ff599e3, 0x9d9e1ae0, 0xd3d3e1ab, 0x21b862a8,
     0x32e8915c, 0xc083125f, 0x144976b4, 0xe622f5b7, 0xf5720643, 0x07198540,
     0x590ab964, 0xab613a67, 0xb831c993, 0x4a5a4a90, 0x9e902e7b, 0x6cfbad78,
     0x7fab5e8c, 0x8dc0dd8f, 0xe330a81a, 0x115b2b19, 0x020bd8ed, 0xf0605bee,
     0x24aa3f05, 0xd6c1bc06, 0xc5914ff2, 0x37faccf1, 0x69e9f0d5, 0x9b8273d6,
     0x88d28022, 0x7ab90321, 0xae7367ca, 0x5c18e4c9, 0x4f48173d, 0xbd23943e,
     0xf36e6f75, 0x0105ec76, 0x12551f82, 0xe03e9c81, 0x34f4f86a, 0xc69f7b69,
     0xd5cf889d, 0x27a40b9e, 0x79b737ba, 0x8bdcb4b9, 0x988c474d, 0x6ae7c44e,
     0xbe2da0a5, 0x4c4623a6, 0x5f16d052, 0xad7d5351},
    {0x00000000, 0x13a29877, 0x274530ee, 0x34e7a899, 0x4e8a61dc, 0x5d28f9ab,
     0x69cf5132, 0x7a6dc945, 0x9d14c3b8, 0x8eb65bcf, 0xba51f356, 0xa9f36b21,
     0xd39ea264, 0xc03c3a13, 0xf4db928a, 0xe7790afd, 0x3fc5f181, 0x2c6769f6,
     0x1880c16f, 0x0b225918, 0x714f905d, 0x62ed082a, 0x560aa0b3, 0x45a838c4,
     0xa2d13239, 0xb173aa4e, 0x859402d7, 0x96369aa0, 0xec5b53e5, 0xfff9cb92,
     0xcb1e630b, 0xd8bcfb7c, 0x7f8be302, 0x6c297b75, 0x58ced3ec, 0x4b6c4b9b,
     0x310182de, 0x22a31aa9, 0x1644b230, 0x05e62a47, 0xe29f20ba, 0xf13db8cd,
     0xc5da1054, 0xd6788823, 0xac154166, 0xbfb7d911, 0x8b507188, 0x98f2e9ff,
     0x404e1283, 0x53ec8af4, 0x670b226d, 0x74a9ba1a, 0x0ec4735f, 0x1d66eb28,
     0x298143b1, 0x3a23dbc6, 0xdd5ad13b, 0xcef8494c, 0xfa1fe1d5, 0xe9bd79a2,
     0x93d0b0e7, 0x80722890, 0xb4958009, 0xa737187e, 0xff17c604, 0xecb55e73,
     0xd852f6ea, 0xcbf06e9d, 0xb19da7d8, 0xa23f3faf, 0x96d89736, 0x857a0f41,
     0x620305bc, 0x71a19dcb, 0x45463552, 0x56e4ad25, 0x2c896460, 0x3f2bfc17,
     0x0bcc548e, 0x186eccf9, 0xc0d23785, 0xd370aff2, 0xe797076b, 0xf4359f1c,
     0x8e585659, 0x9dface2e, 0xa91d66b7, 0xbabffec0, 0x5dc6f43d, 0x4e646c4a,
     0x7a83c4d3, 0x69215ca4, 0x134c95e1, 0x00ee0d96, 0x3409a50f, 0x27ab3d78,
     0x809c2506, 0x933ebd71, 0xa7d915e8, 0xb47b8d9f, 0xce1644da, 0xddb4dcad,
     0xe9537434, 0xfaf1ec43, 0x1d88e6be, 0x0e2a7ec9, 0x3acdd650, 0x296f4e27,
     0x53028762, 0x40a01f15, 0x7447b78c, 0x67e52ffb, 0xbf59d487, 0xacfb4cf0,
     0x981ce469, 0x8bbe7c1e, 0xf1d3b55b, 0xe2712d2c, 0xd69685b5, 0xc5341dc2,
     0x224d173f, 0x31ef8f48, 0x050827d1, 0x16aabfa6, 0x6cc776e3, 0x7f65ee94,
     0x4b82460d, 0x5820de7a, 0xfbc3faf9, 0xe861628e, 0xdc86ca17, 0xcf245260,
     0xb5499b25, 0xa6eb0352, 0x920cabcb, 0x81ae33bc, 0x66d73941, 0x7575a136,
     0x419209af, 0x523091d8, 0x285d589d, 0x3bffc0ea, 0x0f186873, 0x1cbaf004,
     0xc4060b78, 0xd7a4930f, 0xe3433b96, 0xf0e1a3e1, 0x8a8c6aa4, 0x992ef2d3,
     0xadc95a4a, 0xbe6bc23d, 0x5912c8c0, 0x4ab050b7, 0x7e57f82e, 0x6df56059,
     0x1798a91c, 0x043a316b, 0x30dd99f2, 0x237f0185, 0x844819fb, 0x97ea818c,
     0xa30d2915, 0xb0afb162, 0xcac27827, 0xd960e050, 0xed8748c9, 0xfe25d0be,
     0x195cda43, 0x0afe4234, 0x3e19eaad, 0x2dbb72da, 0x57d6bb9f, 0x447423e8,
     0x70938b71, 0x63311306, 0xbb8de87a, 0xa82f700d, 0x9cc8d894, 0x8f6a40e3,
     0xf50789a6, 0xe6a511d1, 0xd242b948, 0xc1e0213f, 0x26992bc2, 0x353bb3b5,
     0x01dc1b2c, 0x127e835b, 0x68134a1e, 0x7bb1d269, 0x4f567af0, 0x5cf4e287,
     0x04d43cfd, 0x1776a48a, 0x23910c13, 0x30339464, 0x4a5e5d21, 0x59fcc556,
     0x6d1b6dcf, 0x7eb9f5b8, 0x99c0ff45, 0x8a626732, 0xbe85cfab, 0xad2757dc,
     0xd74a9e99, 0xc4e806ee, 0xf00fae77, 0xe3ad3600, 0x3b11cd7c, 0x28b3550b,
     0x1c54fd92, 0x0ff665e5, 0x759baca0, 0x663934d7, 0x52de9c4e, 0x417c0439,
     0xa6050ec4, 0xb5a796b3, 0x81403e2a, 0x92e2a65d, 0xe88f6f18, 0xfb2df76f,
     0xcfca5ff6, 0xdc68c781, 0x7b5fdfff, 0x68fd4788, 0x5c1aef11, 0x4fb87766,
     0x35d5be23, 0x26772654, 0x12908ecd, 0x013216ba, 0xe64b1c47, 0xf5e98430,
     0xc10e2ca9, 0xd2acb4de, 0xa8c17d9b, 0xbb63e5ec, 0x8f844d75, 0x9c26d502,
     0x449a2e7e, 0x5738b609, 0x63df1e90, 0x707d86e7, 0x0a104fa2, 0x19b2d7d5,
     0x2d557f4c, 0x3ef7e73b, 0xd98eedc6, 0xca2c75b1, 0xfecbdd28, 0xed69455f,
     0x97048c1a, 0x84a6146d, 0xb041bcf4, 0xa3e32483},
    {0x00000000, 0xa541927e, 0x4f6f520d, 0xea2ec073, 0x9edea41a, 0x3b9f3664,
     0xd1b1f617, 0x74f06469, 0x38513ec5, 0x9d10acbb, 0x773e6cc8, 0xd27ffeb6,
     0xa68f9adf, 0x03ce08a1, 0xe9e0c8d2, 0x4ca15aac, 0x70a27d8a, 0xd5e3eff4,
     0x3fcd2f87, 0x9a8cbdf9, 0xee7cd990, 0x4b3d4bee, 0xa1138b9d, 0x045219e3,
     0x48f3434f, 0xedb2d131, 0x079c1142, 0xa2dd833c, 0xd62de755, 0x736c752b,
     0x9942b558, 0x3c032726, 0xe144fb14, 0x4405696a, 0xae2ba919, 0x0b6a3b67,
     0x7f9a5f0e, 0xdadbcd70, 0x30f50d03, 0x95b49f7d, 0xd915c5d1, 0x7c5457af,
     0x967a97dc, 0x333b05a2, 0x47cb61cb, 0xe28af3b5, 0x08a433c6, 0xade5a1b8,
     0x91e6869e, 0x34a714e0, 0xde89d493, 0x7bc846ed, 0x0f382284, 0xaa79b0fa,
     0x40577089, 0xe516e2f7, 0xa9b7b85b, 0x0cf62a25, 0xe6d8ea56, 0x43997828,
     0x37691c41, 0x92288e3f, 0x78064e4c, 0xdd47dc32, 0xc76580d9, 0x622412a7,
     0x880ad2d4, 0x2d4b40aa, 0x59bb24c3, 0xfcfab6bd, 0x16d476ce, 0xb395e4b0,
     0xff34be1c, 0x5a752c62, 0xb05bec11, 0x151a7e6f, 0x61ea1a06, 0xc4ab8878,
     0x2e85480b, 0x8bc4da75, 0xb7c7fd53, 0x12866f2d, 0xf8a8af5e, 0x5de93d20,
     0x29195949, 0x8c58cb37, 0x66760b44, 0xc337993a, 0x8f96c396, 0x2ad751e8,
     0xc0f9919b, 0x65b803e5, 0x1148678c, 0xb409f5f2, 0x5e273581, 0xfb66a7ff,
     0x26217bcd, 0x8360e9b3, 0x694e29c0, 0xcc0fbbbe, 0xb8ffdfd7, 0x1dbe4da9,
     0xf7908dda, 0x52d11fa4, 0x1e704508, 0xbb31d776, 0x511f1705, 0xf45e857b,
     0x80aee112, 0x25ef736c, 0xcfc1b31f, 0x6a802161, 0x56830647, 0xf3c29439,
     0x19ec544a, 0xbcadc634, 0xc85da25d, 0x6d1c3023, 0x8732f050, 0x2273622e,
     0x6ed23882, 0xcb93aafc, 0x21bd6a8f, 0x84fcf8f1, 0xf00c9c98, 0x554d0ee6,
     0xbf63ce95, 0x1a225ceb, 0x8b277743, 0x2e66e53d, 0xc448254e, 0x6109b730,
     0x15f9d359, 0xb0b84127, 0x5a968154, 0xffd7132a, 0xb3764986, 0x1637dbf8,
     0xfc191b8b, 0x595889f5, 0x2da8ed9c, 0x88e97fe2, 0x62c7bf91, 0xc7862def,
     0xfb850ac9, 0x5ec498b7, 0xb4ea58c4, 0x11abcaba, 0x655baed3, 0xc01a3cad,
     0x2a34fcde, 0x8f756ea0, 0xc3d4340c, 0x6695a672, 0x8cbb6601, 0x29faf47f,
     0x5d0a9016, 0xf84b0268, 0x1265c21b, 0xb7245065, 0x6a638c57, 0xcf221e29,
     0x250cde5a, 0x804d4c24, 0xf4bd284d, 0x51fcba33, 0xbbd27a40, 0x1e93e83e,
     0x5232b292, 0xf77320ec, 0x1d5de09f, 0xb81c72e1, 0xccec1688, 0x69ad84f6,
     0x83834485, 0x26c2d6fb, 0x1ac1f1dd, 0xbf8063a3, 0x55aea3d0, 0xf0ef31ae,
     0x841f55c7, 0x215ec7b9, 0xcb7007ca, 0x6e3195b4, 0x2290cf18, 0x87d15d66,
     0x6dff9d15, 0xc8be0f6b, 0xbc4e6b02, 0x190ff97c, 0xf321390f, 0x5660ab71,
     0x4c42f79a, 0xe90365e4, 0x032da597, 0xa66c37e9, 0xd29c5380, 0x77ddc1fe,
     0x9df3018d, 0x38b293f3, 0x7413c95f, 0xd1525b21, 0x3b7c9b52, 0x9e3d092c,
     0xeacd6d45, 0x4f8cff3b, 0xa5a23f48, 0x00e3ad36, 0x3ce08a10, 0x99a1186e,
     0x738fd81d, 0xd6ce4a63, 0xa23e2e0a, 0x077fbc74, 0xed517c07, 0x4810ee79,
     0x04b1b4d5, 0xa1f026ab, 0x4bdee6d8, 0xee9f74a6, 0x9a6f10cf, 0x3f2e82b1,
     0xd50042c2, 0x7041d0bc, 0xad060c8e, 0x08479ef0, 0xe2695e83, 0x4728ccfd,
     0x33d8a894, 0x96993aea, 0x7cb7fa99, 0xd9f668e7, 0x9557324b, 0x3016a035,
     0xda386046, 0x7f79f238, 0x0b899651, 0xaec8042f, 0x44e6c45c, 0xe1a75622,
     0xdda47104, 0x78e5e37a, 0x92cb2309, 0x378ab177, 0x437ad51e, 0xe63b4760,
     0x0c158713, 0xa954156d, 0xe5f54fc1, 0x40b4ddbf, 0xaa9a1dcc, 0x0fdb8fb2,
     0x7b2bebdb, 0xde6a79a5, 0x3444b9d6, 0x91052ba8},
    {0x00000000, 0xdd45aab8, 0xbf672381, 0x62228939, 0x7b2231f3, 0xa6679b4b,
     0xc4451272, 0x1900b8ca, 0xf64463e6, 0x2b01c95e, 0x49234067, 0x9466eadf,
     0x8d665215, 0x5023f8ad, 0x32017194, 0xef44db2c, 0xe964b13d, 0x34211b85,
     0x560392bc, 0x8b463804, 0x924680ce, 0x4f032a76, 0x2d21a34f, 0xf06409f7,
     0x1f20d2db, 0xc2657863, 0xa047f15a, 0x7d025be2, 0x6402e328, 0xb9474990,
     0xdb65c0a9, 0x06206a11, 0xd725148b, 0x0a60be33, 0x6842370a, 0xb5079db2,
     0xac072578, 0x71428fc0, 0x136006f9, 0xce25ac41, 0x2161776d, 0xfc24ddd5,
     0x9e0654ec, 0x4343fe54, 0x5a43469e, 0x8706ec26, 0xe524651f, 0x3861cfa7,
     0x3e41a5b6, 0xe3040f0e, 0x81268637, 0x5c632c8f, 0x45639445, 0x98263efd,
     0xfa04b7c4, 0x27411d7c, 0xc805c650, 0x15406ce8, 0x7762e5d1, 0xaa274f69,
     0xb327f7a3, 0x6e625d1b, 0x0c40d422, 0xd1057e9a, 0xaba65fe7, 0x76e3f55f,
     0x14c17c66, 0xc984d6de, 0xd0846e14, 0x0dc1c4ac, 0x6fe34d95, 0xb2a6e72d,
     0x5de23c01, 0x80a796b9, 0xe2851f80, 0x3fc0b538, 0x26c00df2, 0xfb85a74a,
     0x99a72e73, 0x44e284cb, 0x42c2eeda, 0x9f874462, 0xfda5cd5b, 0x20e067e3,
     0x39e0df29, 0xe4a57591, 0x8687fca8, 0x5bc25610, 0xb4868d3c, 0x69c32784,
     0x0be1aebd, 0xd6a40405, 0xcfa4bccf, 0x12e11677, 0x70c39f4e, 0xad8635f6,
     0x7c834b6c, 0xa1c6e1d4, 0xc3e468ed, 0x1ea1c255, 0x07a17a9f, 0xdae4d027,
     0xb8c6591e, 0x6583f3a6, 0x8ac7288a, 0x57828232, 0x35a00b0b, 0xe8e5a1b3,
     0xf1e51979, 0x2ca0b3c1, 0x4e823af8, 0x93c79040, 0x95e7fa51, 0x48a250e9,
     0x2a80d9d0, 0xf7c57368, 0xeec5cba2, 0x3380611a, 0x51a2e823, 0x8ce7429b,
     0x63a399b7, 0xbee6330f, 0xdcc4ba36, 0x0181108e, 0x1881a844, 0xc5c402fc,
     0xa7e68bc5, 0x7aa3217d, 0x52a0c93f, 0x8fe56387, 0xedc7eabe, 0x30824006,
     0x2982f8cc, 0xf4c75274, 0x96e5db4d, 0x4ba071f5, 0xa4e4aad9, 0x79a10061,
     0x1b838958, 0xc6c623e0, 0xdfc69b2a, 0x02833192, 0x60a1b8ab, 0xbde41213,
     0xbbc47802, 0x6681d2ba, 0x04a35b83, 0xd9e6f13b, 0xc0e649f1, 0x1da3e349,
     0x7f816a70, 0xa2c4c0c8, 0x4d801be4, 0x90c5b15c, 0xf2e73865, 0x2fa292dd,
     0x36a22a17, 0xebe780af, 0x89c50996, 0x5480a32e, 0x8585ddb4, 0x58c0770c,
     0x3ae2fe35, 0xe7a7548d, 0xfea7ec47, 0x23e246ff, 0x41c0cfc6, 0x9c85657e,
     0x73c1be52, 0xae8414ea, 0xcca69dd3, 0x11e3376b, 0x08e38fa1, 0xd5a62519,
     0xb784ac20, 0x6ac10698, 0x6ce16c89, 0xb1a4c631, 0xd3864f08, 0x0ec3e5b0,
     0x17c35d7a, 0xca86f7c2, 0xa8a47efb, 0x75e1d443, 0x9aa50f6f, 0x47e0a5d7,
     0x25c22cee, 0xf8878656, 0xe1873e9c, 0x3cc29424, 0x5ee01d1d, 0x83a5b7a5,
     0xf90696d8, 0x24433c60, 0x4661b559, 0x9b241fe1, 0x8224a72b, 0x5f610d93,
     0x3d4384aa, 0xe0062e12, 0x0f42f53e, 0xd2075f86, 0xb025d6bf, 0x6d607c07,
     0x7460c4cd, 0xa9256e75, 0xcb07e74c, 0x16424df4, 0x106227e5, 0xcd278d5d,
     0xaf050464, 0x7240aedc, 0x6b401616, 0xb605bcae, 0xd4273597, 0x09629f2f,
     0xe6264403, 0x3b63eebb, 0x59416782, 0x8404cd3a, 0x9d0475f0, 0x4041df48,
     0x22635671, 0xff26fcc9, 0x2e238253, 0xf36628eb, 0x9144a1d2, 0x4c010b6a,
     0x5501b3a0, 0x88441918, 0xea669021, 0x37233a99, 0xd867e1b5, 0x05224b0d,
     0x6700c234, 0xba45688c, 0xa345d046, 0x7e007afe, 0x1c22f3c7, 0xc167597f,
     0xc747336e, 0x1a0299d6, 0x782010ef, 0xa565ba57, 0xbc65029d, 0x6120a825,
     0x0302211c, 0xde478ba4, 0x31035088, 0xec46fa30, 0x8e647309, 0x5321d9b1,
     0x4a21617b, 0x9764cbc3, 0xf54642fa, 0x2803e842},
    {0x00000000, 0x38116fac, 0x7022df58, 0x4833b0f4, 0xe045beb0, 0xd854d11c,
     0x906761e8, 0xa8760e44, 0xc5670b91, 0xfd76643d, 0xb545d4c9, 0x8d54bb65,
     0x2522b521, 0x1d33da8d, 0x55006a79, 0x6d1105d5, 0x8f2261d3, 0xb7330e7f,
     0xff00be8b, 0xc711d127, 0x6f67df63, 0x5776b0cf, 0x1f45003b, 0x27546f97,
     0x4a456a42, 0x725405ee, 0x3a67b51a, 0x0276dab6, 0xaa00d4f2, 0x9211bb5e,
     0xda220baa, 0xe2336406, 0x1ba8b557, 0x23b9dafb, 0x6b8a6a0f, 0x539b05a3,
     0xfbed0be7, 0xc3fc644b, 0x8bcfd4bf, 0xb3debb13, 0xdecfbec6, 0xe6ded16a,
     0xaeed619e, 0x96fc0e32, 0x3e8a0076, 0x069b6fda, 0x4ea8df2e, 0x76b9b082,
     0x948ad484, 0xac9bbb28, 0xe4a80bdc, 0xdcb96470, 0x74cf6a34, 0x4cde0598,
     0x04edb56c, 0x3cfcdac0, 0x51eddf15, 0x69fcb0b9, 0x21cf004d, 0x19de6fe1,
     0xb1a861a5, 0x89b90e09, 0xc18abefd, 0xf99bd151, 0x37516aae, 0x0f400502,
     0x4773b5f6, 0x7f62da5a, 0xd714d41e, 0xef05bbb2, 0xa7360b46, 0x9f2764ea,
     0xf236613f, 0xca270e93, 0x8214be67, 0xba05d1cb, 0x1273df8f, 0x2a62b023,
     0x625100d7, 0x5a406f7b, 0xb8730b7d, 0x806264d1, 0xc851d425, 0xf040bb89,
     0x5836b5cd, 0x6027da61, 0x28146a95, 0x10050539, 0x7d1400ec, 0x45056f40,
     0x0d36dfb4, 0x3527b018, 0x9d51be5c, 0xa540d1f0, 0xed736104, 0xd5620ea8,
     0x2cf9dff9, 0x14e8b055, 0x5cdb00a1, 0x64ca6f0d, 0xccbc6149, 0xf4ad0ee5,
     0xbc9ebe11, 0x848fd1bd, 0xe99ed468, 0xd18fbbc4, 0x99bc0b30, 0xa1ad649c,
     0x09db6ad8, 0x31ca0574, 0x79f9b580, 0x41e8da2c, 0xa3dbbe2a, 0x9bcad186,
     0xd3f96172, 0xebe80ede, 0x439e009a, 0x7b8f6f36, 0x33bcdfc2, 0x0badb06e,
     0x66bcb5bb, 0x5eadda17, 0x169e6ae3, 0x2e8f054f, 0x86f90b0b, 0xbee864a7,
     0xf6dbd453, 0xcecabbff, 0x6ea2d55c, 0x56b3baf0, 0x1e800a04, 0x269165a8,
     0x8ee76bec, 0xb6f60440, 0xfec5b4b4, 0xc6d4db18, 0xabc5decd, 0x93d4b161,
     0xdbe70195, 0xe3f66e39, 0x4b80607d, 0x73910fd1, 0x3ba2bf25, 0x03b3d089,
     0xe180b48f, 0xd991db23, 0x91a26bd7, 0xa9b3047b, 0x01c50a3f, 0x39d46593,
     0x71e7d567, 0x49f6bacb, 0x24e7bf1e, 0x1cf6d0b2, 0x54c56046, 0x6cd40fea,
     0xc4a201ae, 0xfcb36e02, 0xb480def6, 0x8c91b15a, 0x750a600b, 0x4d1b0fa7,
     0x0528bf53, 0x3d39d0ff, 0x954fdebb, 0xad5eb117, 0xe56d01e3, 0xdd7c6e4f,
     0xb06d6b9a, 0x887c0436, 0xc04fb4c2, 0xf85edb6e, 0x5028d52a, 0x6839ba86,
     0x200a0a72, 0x181b65de, 0xfa2801d8, 0xc2396e74, 0x8a0ade80, 0xb21bb12c,
     0x1a6dbf68, 0x227cd0c4, 0x6a4f6030, 0x525e0f9c, 0x3f4f0a49, 0x075e65e5,
     0x4f6dd511, 0x777cbabd, 0xdf0ab4f9, 0xe71bdb55, 0xaf286ba1, 0x9739040d,
     0x59f3bff2, 0x61e2d05e, 0x29d160aa, 0x11c00f06, 0xb9b60142, 0x81a76eee,
     0xc994de1a, 0xf185b1b6, 0x9c94b463, 0xa485dbcf, 0xecb66b3b, 0xd4a70497,
     0x7cd10ad3, 0x44c0657f, 0x0cf3d58b, 0x34e2ba27, 0xd6d1de21, 0xeec0b18d,
     0xa6f30179, 0x9ee26ed5, 0x36946091, 0x0e850f3d, 0x46b6bfc9, 0x7ea7d065,
     0x13b6d5b0, 0x2ba7ba1c, 0x63940ae8, 0x5b856544, 0xf3f36b00, 0xcbe204ac,
     0x83d1b458, 0xbbc0dbf4, 0x425b0aa5, 0x7a4a6509, 0x3279d5fd, 0x0a68ba51,
     0xa21eb415, 0x9a0fdbb9, 0xd23c6b4d, 0xea2d04e1, 0x873c0134, 0xbf2d6e98,
     0xf71ede6c, 0xcf0fb1c0, 0x6779bf84, 0x5f68d028, 0x175b60dc, 0x2f4a0f70,
     0xcd796b76, 0xf56804da, 0xbd5bb42e, 0x854adb82, 0x2d3cd5c6, 0x152dba6a,
     0x5d1e0a9e, 0x650f6532, 0x081e60e7, 0x300f0f4b, 0x783cbfbf, 0x402dd013,
     0xe85bde57, 0xd04ab1fb, 0x9879010f, 0xa0686ea3},
    {0x00000000, 0xef306b19, 0xdb8ca0c3, 0x34bccbda, 0xb2f53777, 0x5dc55c6e,
     0x697997b4, 0x8649fcad, 0x6006181f, 0x8f367306, 0xbb8ab8dc, 0x54bad3c5,
     0xd2f32f68, 0x3dc34471, 0x097f8fab, 0xe64fe4b2, 0xc00c303e, 0x2f3c5b27,
     0x1b8090fd, 0xf4b0fbe4, 0x72f90749, 0x9dc96c50, 0xa975a78a, 0x4645cc93,
     0xa00a2821, 0x4f3a4338, 0x7b8688e2, 0x94b6e3fb, 0x12ff1f56, 0xfdcf744f,
     0xc973bf95, 0x2643d48c, 0x85f4168d, 0x6ac47d94, 0x5e78b64e, 0xb148dd57,
     0x370121fa, 0xd8314ae3, 0xec8d8139, 0x03bdea20, 0xe5f20e92, 0x0ac2658b,
     0x3e7eae51, 0xd14ec548, 0x570739e5, 0xb83752fc, 0x8c8b9926, 0x63bbf23f,
     0x45f826b3, 0xaac84daa, 0x9e748670, 0x7144ed69, 0xf70d11c4, 0x183d7add,
     0x2c81b107, 0xc3b1da1e, 0x25fe3eac, 0xcace55b5, 0xfe729e6f, 0x1142f576,
     0x970b09db, 0x783b62c2, 0x4c87a918, 0xa3b7c201, 0x0e045beb, 0xe13430f2,
     0xd588fb28, 0x3ab89031, 0xbcf16c9c, 0x53c10785, 0x677dcc5f, 0x884da746,
     0x6e0243f4, 0x813228ed, 0xb58ee337, 0x5abe882e, 0xdcf77483, 0x33c71f9a,
     0x077bd440, 0xe84bbf59, 0xce086bd5, 0x213800cc, 0x1584cb16, 0xfab4a00f,
     0x7cfd5ca2, 0x93cd37bb, 0xa771fc61, 0x48419778, 0xae0e73ca, 0x413e18d3,
     0x7582d309, 0x9ab2b810, 0x1cfb44bd, 0xf3cb2fa4, 0xc777e47e, 0x28478f67,
     0x8bf04d66, 0x64c0267f, 0x507ceda5, 0xbf4c86bc, 0x39057a11, 0xd6351108,
     0xe289dad2, 0x0db9b1cb, 0xebf65579, 0x04c63e60, 0x307af5ba, 0xdf4a9ea3,
     0x5903620e, 0xb6330917, 0x828fc2cd, 0x6dbfa9d4, 0x4bfc7d58, 0xa4cc1641,
     0x9070dd9b, 0x7f40b682, 0xf9094a2f, 0x16392136, 0x2285eaec, 0xcdb581f5,
     0x2bfa6547, 0xc4ca0e5e, 0xf076c584, 0x1f46ae9d, 0x990f5230, 0x763f3929,
     0x4283f2f3, 0xadb399ea, 0x1c08b7d6, 0xf338dccf, 0xc7841715, 0x28b47c0c,
     0xaefd80a1, 0x41cdebb8, 0x75712062, 0x9a414b7b, 0x7c0eafc9, 0x933ec4d0,
     0xa7820f0a, 0x48b26413, 0xcefb98be, 0x21cbf3a7, 0x1577387d, 0xfa475364,
     0xdc0487e8, 0x3334ecf1, 0x0788272b, 0xe8b84c32, 0x6ef1b09f, 0x81c1db86,
     0xb57d105c, 0x5a4d7b45, 0xbc029ff7, 0x5332f4ee, 0x678e3f34, 0x88be542d,
     0x0ef7a880, 0xe1c7c399, 0xd57b0843, 0x3a4b635a, 0x99fca15b, 0x76ccca42,
     0x42700198, 0xad406a81, 0x2b09962c, 0xc439fd35, 0xf08536ef, 0x1fb55df6,
     0xf9fab944, 0x16cad25d, 0x22761987, 0xcd46729e, 0x4b0f8e33, 0xa43fe52a,
     0x90832ef0, 0x7fb345e9, 0x59f09165, 0xb6c0fa7c, 0x827c31a6, 0x6d4c5abf,
     0xeb05a612, 0x0435cd0b, 0x308906d1, 0xdfb96dc8, 0x39f6897a, 0xd6c6e263,
     0xe27a29b9, 0x0d4a42a0, 0x8b03be0d, 0x6433d514, 0x508f1ece, 0xbfbf75d7,
     0x120cec3d, 0xfd3c8724, 0xc9804cfe, 0x26b027e7, 0xa0f9db4a, 0x4fc9b053,
     0x7b757b89, 0x94451090, 0x720af422, 0x9d3a9f3b, 0xa98654e1, 0x46b63ff8,
     0xc0ffc355, 0x2fcfa84c, 0x1b736396, 0xf443088f, 0xd200dc03, 0x3d30b71a,
     0x098c7cc0, 0xe6bc17d9, 0x60f5eb74, 0x8fc5806d, 0xbb794bb7, 0x544920ae,
     0xb206c41c, 0x5d36af05, 0x698a64df, 0x86ba0fc6, 0x00f3f36b, 0xefc39872,
     0xdb7f53a8, 0x344f38b1, 0x97f8fab0, 0x78c891a9, 0x4c745a73, 0xa344316a,
     0x250dcdc7, 0xca3da6de, 0xfe816d04, 0x11b1061d, 0xf7fee2af, 0x18ce89b6,
     0x2c72426c, 0xc3422975, 0x450bd5d8, 0xaa3bbec1, 0x9e87751b, 0x71b71e02,
     0x57f4ca8e, 0xb8c4a197, 0x8c786a4d, 0x63480154, 0xe501fdf9, 0x0a3196e0,
     0x3e8d5d3a, 0xd1bd3623, 0x37f2d291, 0xd8c2b988, 0xec7e7252, 0x034e194b,
     0x8507e5e6, 0x6a378eff, 0x5e8b4525, 0xb1bb2e3c},
    {0x00000000, 0x68032cc8, 0xd0065990, 0xb8057558, 0xa5e0c5d1, 0xcde3e919,
     0x75e69c41, 0x1de5b089, 0x4e2dfd53, 0x262ed19b, 0x9e2ba4c3, 0xf628880b,
     0xebcd3882, 0x83ce144a, 0x3bcb6112, 0x53c84dda, 0x9c5bfaa6, 0xf458d66e,
     0x4c5da336, 0x245e8ffe, 0x39bb3f77, 0x51b813bf, 0xe9bd66e7, 0x81be4a2f,
     0xd27607f5, 0xba752b3d, 0x02705e65, 0x6a7372ad, 0x7796c224, 0x1f95eeec,
     0xa7909bb4, 0xcf93b77c, 0x3d5b83bd, 0x5558af75, 0xed5dda2d, 0x855ef6e5,
     0x98bb466c, 0xf0b86aa4, 0x48bd1ffc, 0x20be3334, 0x73767eee, 0x1b755226,
     0xa370277e, 0xcb730bb6, 0xd696bb3f, 0xbe9597f7, 0x0690e2af, 0x6e93ce67,
     0xa100791b, 0xc90355d3, 0x7106208b, 0x19050c43, 0x04e0bcca, 0x6ce39002,
     0xd4e6e55a, 0xbce5c992, 0xef2d8448, 0x872ea880, 0x3f2bddd8, 0x5728f110,
     0x4acd4199, 0x22ce6d51, 0x9acb1809, 0xf2c834c1, 0x7ab7077a, 0x12b42bb2,
     0xaab15eea, 0xc2b27222, 0xdf57c2ab, 0xb754ee63, 0x0f519b3b, 0x6752b7f3,
     0x349afa29, 0x5c99d6e1, 0xe49ca3b9, 0x8c9f8f71, 0x917a3ff8, 0xf9791330,
     0x417c6668, 0x297f4aa0, 0xe6ecfddc, 0x8eefd114, 0x36eaa44c, 0x5ee98884,
     0x430c380d, 0x2b0f14c5, 0x930a619d, 0xfb094d55, 0xa8c1008f, 0xc0c22c47,
     0x78c7591f, 0x10c475d7, 0x0d21c55e, 0x6522e996, 0xdd279cce, 0xb524b006,
     0x47ec84c7, 0x2fefa80f, 0x97eadd57, 0xffe9f19f, 0xe20c4116, 0x8a0f6dde,
     0x320a1886, 0x5a09344e, 0x09c17994, 0x61c2555c, 0xd9c72004, 0xb1c40ccc,
     0xac21bc45, 0xc422908d, 0x7c27e5d5, 0x1424c91d, 0xdbb77e61, 0xb3b452a9,
     0x0bb127f1, 0x63b20b39, 0x7e57bbb0, 0x16549778, 0xae51e220, 0xc652cee8,
     0x959a8332, 0xfd99affa, 0x459cdaa2, 0x2d9ff66a, 0x307a46e3, 0x58796a2b,
     0xe07c1f73, 0x887f33bb, 0xf56e0ef4, 0x9d6d223c, 0x25685764, 0x4d6b7bac,
     0x508ecb25, 0x388de7ed, 0x808892b5, 0xe88bbe7d, 0xbb43f3a7, 0xd340df6f,
     0x6b45aa37, 0x034686ff, 0x1ea33676, 0x76a01abe, 0xcea56fe6, 0xa6a6432e,
     0x6935f452, 0x0136d89a, 0xb933adc2, 0xd130810a, 0xccd53183, 0xa4d61d4b,
     0x1cd36813, 0x74d044db, 0x27180901, 0x4f1b25c9, 0xf71e5091, 0x9f1d7c59,
     0x82f8ccd0, 0xeafbe018, 0x52fe9540, 0x3afdb988, 0xc8358d49, 0xa036a181,
     0x1833d4d9, 0x7030f811, 0x6dd54898, 0x05d66450, 0xbdd31108, 0xd5d03dc0,
     0x8618701a, 0xee1b5cd2, 0x561e298a, 0x3e1d0542, 0x23f8b5cb, 0x4bfb9903,
     0xf3feec5b, 0x9bfdc093, 0x546e77ef, 0x3c6d5b27, 0x84682e7f, 0xec6b02b7,
     0xf18eb23e, 0x998d9ef6, 0x2188ebae, 0x498bc766, 0x1a438abc, 0x7240a674,
     0xca45d32c, 0xa246ffe4, 0xbfa34f6d, 0xd7a063a5, 0x6fa516fd, 0x07a63a35,
     0x8fd9098e, 0xe7da2546, 0x5fdf501e, 0x37dc7cd6, 0x2a39cc5f, 0x423ae097,
     0xfa3f95cf, 0x923cb907, 0xc1f4f4dd, 0xa9f7d815, 0x11f2ad4d, 0x79f18185,
     0x6414310c, 0x0c171dc4, 0xb412689c, 0xdc114454, 0x1382f328, 0x7b81dfe0,
     0xc384aab8, 0xab878670, 0xb66236f9, 0xde611a31, 0x66646f69, 0x0e6743a1,
     0x5daf0e7b, 0x35ac22b3, 0x8da957eb, 0xe5aa7b23, 0xf84fcbaa, 0x904ce762,
     0x2849923a, 0x404abef2, 0xb2828a33, 0xda81a6fb, 0x6284d3a3, 0x0a87ff6b,
     0x17624fe2, 0x7f61632a, 0xc7641672, 0xaf673aba, 0xfcaf7760, 0x94ac5ba8,
     0x2ca92ef0, 0x44aa0238, 0x594fb2b1, 0x314c9e79, 0x8949eb21, 0xe14ac7e9,
     0x2ed97095, 0x46da5c5d, 0xfedf2905, 0x96dc05cd, 0x8b39b544, 0xe33a998c,
     0x5b3fecd4, 0x333cc01c, 0x60f48dc6, 0x08f7a10e, 0xb0f2d456, 0xd8f1f89e,
     0xc5144817, 0xad1764df, 0x15121187, 0x7d113d4f},
    {0x00000000, 0x493c7d27, 0x9278fa4e, 0xdb448769, 0x211d826d, 0x6821ff4a,
     0xb3657823, 0xfa590504, 0x423b04da, 0x0b0779fd, 0xd043fe94, 0x997f83b3,
     0x632686b7, 0x2a1afb90, 0xf15e7cf9, 0xb86201de, 0x847609b4, 0xcd4a7493,
     0x160ef3fa, 0x5f328edd, 0xa56b8bd9, 0xec57f6fe, 0x37137197, 0x7e2f0cb0,
     0xc64d0d6e, 0x8f717049, 0x5435f720, 0x1d098a07, 0xe7508f03, 0xae6cf224,
     0x7528754d, 0x3c14086a, 0x0d006599, 0x443c18be, 0x9f789fd7, 0xd644e2f0,
     0x2c1de7f4, 0x65219ad3, 0xbe651dba, 0xf759609d, 0x4f3b6143, 0x06071c64,
     0xdd439b0d, 0x947fe62a, 0x6e26e32e, 0x271a9e09, 0xfc5e1960, 0xb5626447,
     0x89766c2d, 0xc04a110a, 0x1b0e9663, 0x5232eb44, 0xa86bee40, 0xe1579367,
     0x3a13140e, 0x732f6929, 0xcb4d68f7, 0x827115d0, 0x593592b9, 0x1009ef9e,
     0xea50ea9a, 0xa36c97bd, 0x782810d4, 0x31146df3, 0x1a00cb32, 0x533cb615,
     0x8878317c, 0xc1444c5b, 0x3b1d495f, 0x72213478, 0xa965b311, 0xe059ce36,
     0x583bcfe8, 0x1107b2cf, 0xca4335a6, 0x837f4881, 0x79264d85, 0x301a30a2,
     0xeb5eb7cb, 0xa262caec, 0x9e76c286, 0xd74abfa1, 0x0c0e38c8, 0x453245ef,
     0xbf6b40eb, 0xf6573dcc, 0x2d13baa5, 0x642fc782, 0xdc4dc65c, 0x9571bb7b,
     0x4e353c12, 0x07094135, 0xfd504431, 0xb46c3916, 0x6f28be7f, 0x2614c358,
     0x1700aeab, 0x5e3cd38c, 0x857854e5, 0xcc4429c2, 0x361d2cc6, 0x7f2151e1,
     0xa465d688, 0xed59abaf, 0x553baa71, 0x1c07d756, 0xc743503f, 0x8e7f2d18,
     0x7426281c, 0x3d1a553b, 0xe65ed252, 0xaf62af75, 0x9376a71f, 0xda4ada38,
     0x010e5d51, 0x48322076, 0xb26b2572, 0xfb575855, 0x2013df3c, 0x692fa21b,
     0xd14da3c5, 0x9871dee2, 0x4335598b, 0x0a0924ac, 0xf05021a8, 0xb96c5c8f,
     0x6228dbe6, 0x2b14a6c1, 0x34019664, 0x7d3deb43, 0xa6796c2a, 0xef45110d,
     0x151c1409, 0x5c20692e, 0x8764ee47, 0xce589360, 0x763a92be, 0x3f06ef99,
     0xe44268f0, 0xad7e15d7, 0x572710d3, 0x1e1b6df4, 0xc55fea9d, 0x8c6397ba,
     0xb0779fd0, 0xf94be2f7, 0x220f659e, 0x6b3318b9, 0x916a1dbd, 0xd856609a,
     0x0312e7f3, 0x4a2e9ad4, 0xf24c9b0a, 0xbb70e62d, 0x60346144, 0x29081c63,
     0xd3511967, 0x9a6d6440, 0x4129e329, 0x08159e0e, 0x3901f3fd, 0x703d8eda,
     0xab7909b3, 0xe2457494, 0x181c7190, 0x51200cb7, 0x8a648bde, 0xc358f6f9,
     0x7b3af727, 0x32068a00, 0xe9420d69, 0xa07e704e, 0x5a27754a, 0x131b086d,
     0xc85f8f04, 0x8163f223, 0xbd77fa49, 0xf44b876e, 0x2f0f0007, 0x66337d20,
     0x9c6a7824, 0xd5560503, 0x0e12826a, 0x472eff4d, 0xff4cfe93, 0xb67083b4,
     0x6d3404dd, 0x240879fa, 0xde517cfe, 0x976d01d9, 0x4c2986b0, 0x0515fb97,
     0x2e015d56, 0x673d2071, 0xbc79a718, 0xf545da3f, 0x0f1cdf3b, 0x4620a21c,
     0x9d642575, 0xd4585852, 0x6c3a598c, 0x250624ab, 0xfe42a3c2, 0xb77edee5,
     0x4d27dbe1, 0x041ba6c6, 0xdf5f21af, 0x96635c88, 0xaa7754e2, 0xe34b29c5,
     0x380faeac, 0x7133d38b, 0x8b6ad68f, 0xc256aba8, 0x19122cc1, 0x502e51e6,
     0xe84c5038, 0xa1702d1f, 0x7a34aa76, 0x3308d751, 0xc951d255, 0x806daf72,
     0x5b29281b, 0x1215553c, 0x230138cf, 0x6a3d45e8, 0xb179c281, 0xf845bfa6,
     0x021cbaa2, 0x4b20c785, 0x906440ec, 0xd9583dcb, 0x613a3c15, 0x28064132,
     0xf342c65b, 0xba7ebb7c, 0x4027be78, 0x091bc35f, 0xd25f4436, 0x9b633911,
     0xa777317b, 0xee4b4c5c, 0x350fcb35, 0x7c33b612, 0x866ab316, 0xcf56ce31,
     0x14124958, 0x5d2e347f, 0xe54c35a1, 0xac704886, 0x7734cfef, 0x3e08b2c8,
     0xc451b7cc, 0x8d6dcaeb, 0x56294d82, 0x1f1530a5}};

/// @brief Detection of Intel SSE4.2 extensions at runtime:
#if ENABLE_ASM_CRC32 == 1

#include <cpuid.h>
#include <x86intrin.h>

static bool HasSSE42() {
  unsigned int eax, ebx, ecx, edx;
  if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
    if ((ecx & 0x100000) != 0) {
      return true;
    } else {
      return false;
    }
  } else {
    return false;
  }
}

// The following is currently not used, it is a fallback for the case
// that the pure assembler source cannot be assembled or linked for
// some reason.

#if 0
extern "C" uint32_t TRI_BlockCrc32_intrinsics(uint32_t value,
                                              char const* data,
                                              size_t length) {
  uint64_t tmp = value;
  auto p = reinterpret_cast<uint64_t const*>(data);
  while (length >= 8) {
    tmp = _mm_crc32_u64(tmp, *p++);
    length -= 8;
  }
  auto p2 = reinterpret_cast<unsigned char const*>(p);
  value = static_cast<uint32_t>(tmp);
  while (length-- > 0) {
    value = _mm_crc32_u8(value, *p2++);
  }
  return value;
}
#endif

#endif

/// @brief CRC32 value of data block
///
/// optimized code to process 8 bytes at a time. provides a substantial speedup
/// compared to the one-byte-at-a-time version
///
/// code found at http://create.stephan-brumme.com/crc32/#slicing-by-8-overview,
/// original source is http://sourceforge.net/projects/slicing-by-8/
extern "C" {

uint32_t TRI_BlockCrc32_C(uint32_t value, char const* data, size_t length) {
#ifndef TRI_UNALIGNED_ACCESS
  // byte-wise hashing to support platforms that don't permit
  // unaligned accesses of uint32_t values
  uint8_t const* currentChar = reinterpret_cast<uint8_t const*>(data);
  while (length--) {
    value = (value >> 8) ^ Crc32Lookup[0][(value & 0xFF) ^ *currentChar++];
  }
  return value;
#else
  // hash two words in parallel
  uint32_t const* current = reinterpret_cast<uint32_t const*>(data);

  // process eight bytes at once
  while (length >= 8) {
    uint32_t one = *current++ ^ value;
    uint32_t two = *current++;

    value = Crc32Lookup[0][(two >> 24) & 0xFF] ^ Crc32Lookup[1][(two >> 16) & 0xFF] ^
            Crc32Lookup[2][(two >> 8) & 0xFF] ^ Crc32Lookup[3][two & 0xFF] ^
            Crc32Lookup[4][(one >> 24) & 0xFF] ^ Crc32Lookup[5][(one >> 16) & 0xFF] ^
            Crc32Lookup[6][(one >> 8) & 0xFF] ^ Crc32Lookup[7][one & 0xFF];
    length -= 8;
  }

  uint8_t const* currentChar = reinterpret_cast<uint8_t const*>(current);
  // remaining 1 to 7 bytes (standard CRC table-based algorithm)
  while (length--) {
    value = (value >> 8) ^ Crc32Lookup[0][(value & 0xFF) ^ *currentChar++];
  }
  return value;
#endif
}

#if ENABLE_ASM_CRC32 == 1
static uint32_t TRI_BlockCrc32_Detect(uint32_t hash, char const* data, size_t length) {
  if (HasSSE42()) {
    TRI_BlockCrc32 = TRI_BlockCrc32_SSE42;
    // TRI_BlockCrc32 = TRI_BlockCrc32_intrinsics;
  } else {
    TRI_BlockCrc32 = TRI_BlockCrc32_C;
  }
  return (*TRI_BlockCrc32)(hash, data, length);
}

uint32_t (*TRI_BlockCrc32)(uint32_t hash, char const* data, size_t length) = TRI_BlockCrc32_Detect;
#else
uint32_t (*TRI_BlockCrc32)(uint32_t hash, char const* data, size_t length) = TRI_BlockCrc32_C;
#endif

}  // extern "C"

/// @brief computes a CRC32 for memory blobs
uint32_t TRI_Crc32HashPointer(void const* data, size_t length) {
  uint32_t crc = TRI_InitialCrc32();
  crc = TRI_BlockCrc32(crc, static_cast<char const*>(data), length);

  return TRI_FinalCrc32(crc);
}

/// @brief computes a CRC32 for strings
uint32_t TRI_Crc32HashString(char const* data) {
  return TRI_Crc32HashPointer(data, strlen(data));
}
