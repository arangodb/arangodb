/////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Richard Parker
////////////////////////////////////////////////////////////////////////////////

/* TRI_BlockCrc32_SSE42 assembler*/
/* assumes SSE4.2 flag set       */

/*         input                 */
/*  %rdi     uint32_t            */
/*  %rsi     char const* data    */
/*  %rdx     size_t length       */

/*         output                */
/*  %rax     uint32_t            */

/*         working               */
/*  %r10 is the remaining length */
/*  %r8, %r9, %r11 chunk-CRCs    */
/*  %rcx holds address of crct2  */
/*  %edi %rdx used for x^1024    */
/*  no registers spilled         */

/* This routine has been tuned extensively on Haswell  */
/* architecture.  If the data is coming from cache, it */
/* does about 3.8 GB/clock, or 6.58 GB on my 1.7 GHz   */
/* laptop for data length median 80, mean 300 bytes    */

	.text
	.globl	TRI_BlockCrc32_SSE42
	.globl	_TRI_BlockCrc32_SSE42
	.type	TRI_BlockCrc32_SSE42, @function
	.type	_TRI_BlockCrc32_SSE42, @function
        .align  32        /* primarily to stabilize times   */
TRI_BlockCrc32_SSE42:     /* entry point                    */
_TRI_BlockCrc32_SSE42:

        cmpq    $12,%rdx   /* if the string < 12 bytes long */
               /* note #1, #2 threatens segfault otherwise  */
/* 12-15 bytes first fetch OK, second fetch uses all bytes  */
        jb      crca7      /* use the simple approach       */

/*  Next section does 8-15 bytes and adjusts registers      */
/*  length is now in %r10 to free the ancient register %rdx */

        movq    %rdx,%r10  /* copy length to %r10 for later */
        andq    $7,%rdx    /* get length mod 8              */
        lea     crct1(%rip),%r9  /* get address of crct1 table    */
        movl    (%r9,%rdx,4),%ecx  /* table lookup to %ecx  */
        subq    %rdx,%r10  /* subtract 0-7 from length      */
        subq    $8,%r10    /* and subtract another 8        */

        xorq    (%rsi),%rdi  /* #1 xor in data to old CRC   */
        shlq    %cl,%rdi   /* shift amount from table       */
        movzbl  %ch,%edx   /* get length (ancient regs)     */
        addq    %rdx,%rsi  /* update pointer by table-bytes */
        movq    $0,%rax    /* clear old crc - already in    */
        crc32   %rdi,%rax  /* do 4-8 bytes of CRC           */

        shrq    $16,%rcx   /* get other two bytes of table  */

        movq    (%rsi),%r8 /* #2 get data to xor            */
        xorq    %rax,%r8   /*  . . . into the current one   */
        shlq    %cl,%r8    /* shift amount from table       */
        movq    $0,%rax    /* clear old CRC -not used here  */
        crc32   %r8,%rax   /* perform 4-7 byte CRC32        */
        movzbl  %ch,%edx   /* get number of bytes done      */
        addq    %rdx,%rsi  /* and add into pointer          */

/*         at this point we have                 */
/*  %eax hold the CRC of the first few bytes     */
/*  %rsi points to the next chunk of data        */
/*  %r10 is the remaining length, divisible by 8 */

/*  Next step is to prepare computed go-to       */
/*  First get the jump address into %rdi         */

        movq    %r10,%rdx  /* copy the length               */
        andq    $127,%rdx  /* get length mod 128            */
        addq    %rdx,%rsi  /* pointer to next 128 block     */
        addq    $256,%rdx  /* table disp for long strings   */
        cmpq    $256,%r10  /* if length less than 256 . .   */
        cmovb   %r10,%rdx  /* just use length as disp       */
        lea     crctj(%rip),%rdi /* get address of jump table     */
        movq    (%rdi,%rdx),%rdi /* compute jump address    */
        andq    $-128,%r10 /* now get length correct        */
        subq    $128,%r10  /* only used in 256+ case ynn    */
/* *** now comes the major unpredictable jump! ***          */
        jmp     *%rdi      /* go to length-specific code    */

/* The following code is used for strings < 272.  The jump  */
/* above goes to one of the labels and does the entire      */
/* computation (apart from initial 8-15 bytes) in line      */

x248:   crc32q  -120(%rsi),%rax
x240:   crc32q  -112(%rsi),%rax
x232:   crc32q  -104(%rsi),%rax
x224:   crc32q  -96(%rsi),%rax
x216:   crc32q  -88(%rsi),%rax
x208:   crc32q  -80(%rsi),%rax
x200:   crc32q  -72(%rsi),%rax
x192:   crc32q  -64(%rsi),%rax
x184:   crc32q  -56(%rsi),%rax
x176:   crc32q  -48(%rsi),%rax
x168:   crc32q  -40(%rsi),%rax
x160:   crc32q  -32(%rsi),%rax
x152:   crc32q  -24(%rsi),%rax
x144:   crc32q  -16(%rsi),%rax
x136:   crc32q  -8(%rsi),%rax
x128:   crc32q  (%rsi),%rax
        addq    $128,%rsi
x120:   crc32q  -120(%rsi),%rax
x112:   crc32q  -112(%rsi),%rax
x104:   crc32q  -104(%rsi),%rax
x96:    crc32q  -96(%rsi),%rax
x88:    crc32q  -88(%rsi),%rax
x80:    crc32q  -80(%rsi),%rax
x72:    crc32q  -72(%rsi),%rax
x64:    crc32q  -64(%rsi),%rax
x56:    crc32q  -56(%rsi),%rax
x48:    crc32q  -48(%rsi),%rax
x40:    crc32q  -40(%rsi),%rax
x32:    crc32q  -32(%rsi),%rax
x24:    crc32q  -24(%rsi),%rax
x16:    crc32q  -16(%rsi),%rax
x8:     crc32q  -8(%rsi),%rax
x0:     ret

/* The following code starts off by doing up to 120 bytes   */
/* inline (branch to one of the yNN lables) in a simple way */
/* (it then drops into main triple-stream loop)             */

        .align 32
        .8byte 0,0                /* not sure this matters  */
y120:   crc32q  -120(%rsi),%rax
y112:   crc32q  -112(%rsi),%rax
y104:   crc32q  -104(%rsi),%rax
y96:    crc32q  -96(%rsi),%rax
y88:    crc32q  -88(%rsi),%rax
y80:    crc32q  -80(%rsi),%rax
y72:    crc32q  -72(%rsi),%rax
y64:    crc32q  -64(%rsi),%rax
y56:    crc32q  -56(%rsi),%rax
y48:    crc32q  -48(%rsi),%rax
y40:    crc32q  -40(%rsi),%rax
y32:    crc32q  -32(%rsi),%rax
y24:    crc32q  -24(%rsi),%rax
y16:    crc32q  -16(%rsi),%rax
y8:     crc32q  -8(%rsi),%rax
y0:

/*  The following code does blocks of 128 bytes (16 words)  */
/*  The main description is given at the start of the loop  */

/* initialization of the triple-stream method               */

        lea     crct2(%rip),%rcx /* set %rcx permanently as crct2 */
        movq    $0,%r8     /* work out first 11 words of B  */
        crc32q  (%rsi),%r8
        crc32q  8(%rsi),%r8
        crc32q  16(%rsi),%r8
        crc32q  24(%rsi),%r8
        crc32q  32(%rsi),%r8
        crc32q  40(%rsi),%r8
        crc32q  48(%rsi),%r8
        crc32q  56(%rsi),%r8
        crc32q  64(%rsi),%r8
        crc32q  72(%rsi),%r8
        crc32q  80(%rsi),%r8
        movq    $0,%r9     /* work out first six words of C  */
        crc32q  128(%rsi),%r9
        crc32q  136(%rsi),%r9
        crc32q  144(%rsi),%r9
        crc32q  152(%rsi),%r9
        crc32q  160(%rsi),%r9
        crc32q  168(%rsi),%r9
        jmp     crcx7      /* see if we need to do more       */

/* main loop of the triple-stream thread            */

/*  %eax hold the CRC of AAAA                      */
/*  %rsi points to the start of BBBBB...           */
/*  %r10 is the remaining length, divisible by 128 */
/*  %r8 is the CRC of B11                          */
/*  %r9 is the CRC of C6                           */
/*  %r11 will hold the CRC of D                    */
/*  %edi accumulates the shifted CRC of AAAA       */
/*  %rdx holds the 64-bit extended bytes of %eax   */
/*  %rcx holds the address of table crct2          */

/*  we are doing four chunks AAA...BBB...CCC...DDD */
/*  where each chunk is 16 words - 128 bytes       */
/*  AAA has been completed and is multiplied by    */
/*  X^1024 (mod poly) using the lookup tables      */
/*  BBB has had 11 words done - 5 more to do       */
/*  CCC has had 6 words done and 5 more done here  */
/*  DDD is not started, and 6 words are done       */

lp128:

        crc32q  88(%rsi),%r8    /* last five words of B */
        crc32q  96(%rsi),%r8
        crc32q  104(%rsi),%r8
        crc32q  112(%rsi),%r8
        crc32q  120(%rsi),%r8

        crc32q  176(%rsi),%r9   /* middle five words of C */
        crc32q  184(%rsi),%r9
        crc32q  192(%rsi),%r9
        crc32q  200(%rsi),%r9
        crc32q  208(%rsi),%r9

        movq    $0,%r11         /* first six words of D    */
        crc32q  256(%rsi),%r11
        crc32q  264(%rsi),%r11
        crc32q  272(%rsi),%r11
        crc32q  280(%rsi),%r11
        crc32q  288(%rsi),%r11
        crc32q  296(%rsi),%r11

/*  Next chunk muliplies %eax by X^1024 mod poly by looking */
/* up each byte in the table crct2 pointed to by %rcx       */

        movzx   %al,%edx
        movl    0(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    1024(%rcx,%rdx,4),%edi
        shrl    $16,%eax
        movzx   %al,%edx
        xorl    2048(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    3072(%rcx,%rdx,4),%edi
        xorl    %edi,%r8d

/* move chunks up DDD->CCC->BBB->AAA */

        movl    %r8d,%eax
        movl    %r9d,%r8d
        movl    %r11d,%r9d
        addq    $128,%rsi
crcx7:
        subq    $128,%r10
        jne     lp128

/* termination of the triple-stream CRC   */

/* Despite the unpredictable drop through here, it seems */
/* (on Haswell anyway) that decode is easily fast enough */
/* and that "mingling" the following instructions        */
/* gains nothing measurable                              */

        crc32q  88(%rsi),%r8     /*  do last 5 of B      */
        crc32q  96(%rsi),%r8
        crc32q  104(%rsi),%r8
        crc32q  112(%rsi),%r8
        crc32q  120(%rsi),%r8
        movzx   %al,%edx         /* multiply A by X^1024 */
        movl    0(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    1024(%rcx,%rdx,4),%edi
        shrl    $16,%eax
        movzx   %al,%edx
        xorl    2048(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    3072(%rcx,%rdx,4),%edi
        xorl    %edi,%r8d         /* combine with B     */

        movl    %r8d,%eax         /* and prepare for mul*/

        crc32q  176(%rsi),%r9     /* do middle 5 of C   */
        crc32q  184(%rsi),%r9
        crc32q  192(%rsi),%r9
        crc32q  200(%rsi),%r9
        crc32q  208(%rsi),%r9
        crc32q  216(%rsi),%r9     /* and last 5 of C    */
        crc32q  224(%rsi),%r9
        crc32q  232(%rsi),%r9
        crc32q  240(%rsi),%r9
        crc32q  248(%rsi),%r9

        movzx   %al,%edx           /* multiply again X^1024 */
        movl    0(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    1024(%rcx,%rdx,4),%edi
        shrl    $16,%eax
        movzx   %al,%edx
        xorl    2048(%rcx,%rdx,4),%edi
        movzx   %ah,%edx
        xorl    3072(%rcx,%rdx,4),%edi
        xorl    %edi,%r9d          /* combine with C       */
        movl    %r9d,%eax          /* and into return reg  */
        ret

/* This section is only used when there are 0 - 7 bytes     */
/* It works by getting each byte into %cl one at a time     */
/* and using the CRC32 instruction into %rax                */

crca7:
        movl    %edi,%eax  /* move input CRC to output reg  */
        cmpq    $0,%rdx    /* works with length = 0         */
        je      crca9      /* just return when done         */
crca8:
        movb    (%rsi),%cl /* byte of data into a register  */
        crc32   %cl,%eax   /* include that in the CRC       */
        addq    $1,%rsi    /* update pointer for next byte  */
        subq    $1,%rdx    /* one fewer bytes to process    */
        jne     crca8      /* continue until zero left to do*/
crca9:
        ret                /* simple case exit              */

/* crct1 is used to do the initial 8-15 bytes.  It consists */
/* of two <shift,length> pairs.  It ends up in %ecx in the  */
/* sixth instruction executed.                              */
        .align 64
crct1:
        .byte 0x20, 0x04, 0x20, 0x04
        .byte 0x20, 0x04, 0x18, 0x05
        .byte 0x20, 0x04, 0x10, 0x06
        .byte 0x20, 0x04, 0x08, 0x07
        .byte 0x20, 0x04, 0x00, 0x08
        .byte 0x18, 0x05, 0x00, 0x08
        .byte 0x10, 0x06, 0x00, 0x08
        .byte 0x08, 0x07, 0x00, 0x08

/* crctj is the jump table.  One of the addresses is loaded  */
/* from this table and jumped to                             */

crctj:
        .8byte x0
        .8byte x8
        .8byte x16
        .8byte x24
        .8byte x32
        .8byte x40
        .8byte x48
        .8byte x56
        .8byte x64
        .8byte x72
        .8byte x80
        .8byte x88
        .8byte x96
        .8byte x104
        .8byte x112
        .8byte x120
        .8byte x128
        .8byte x136
        .8byte x144
        .8byte x152
        .8byte x160
        .8byte x168
        .8byte x176
        .8byte x184
        .8byte x192
        .8byte x200
        .8byte x208
        .8byte x216
        .8byte x224
        .8byte x232
        .8byte x240
        .8byte x248
        .8byte y0
        .8byte y8
        .8byte y16
        .8byte y24
        .8byte y32
        .8byte y40
        .8byte y48
        .8byte y56
        .8byte y64
        .8byte y72
        .8byte y80
        .8byte y88
        .8byte y96
        .8byte y104
        .8byte y112
        .8byte y120

/* crct2 is the table used to multiply a 32-bit quantity  */
/* by X^1024 modulo the polynomial.  It consists of four  */
/* consecutive tables of 256 32-bit words . . . for the   */
/* least to the most significant bytes of the word        */
/* (notice the bit-reversal done by crc32 throughout!)    */

/* This table is 4K long                                  */

        .align 64
crct2:
      .4byte 0x00000000, 0x6992cea2, 0xd3259d44, 0xbab753e6
      .4byte 0xa3a74c79, 0xca3582db, 0x7082d13d, 0x19101f9f
      .4byte 0x42a2ee03, 0x2b3020a1, 0x91877347, 0xf815bde5
      .4byte 0xe105a27a, 0x88976cd8, 0x32203f3e, 0x5bb2f19c
      .4byte 0x8545dc06, 0xecd712a4, 0x56604142, 0x3ff28fe0
      .4byte 0x26e2907f, 0x4f705edd, 0xf5c70d3b, 0x9c55c399
      .4byte 0xc7e73205, 0xae75fca7, 0x14c2af41, 0x7d5061e3
      .4byte 0x64407e7c, 0x0dd2b0de, 0xb765e338, 0xdef72d9a
      .4byte 0x0f67cefd, 0x66f5005f, 0xdc4253b9, 0xb5d09d1b
      .4byte 0xacc08284, 0xc5524c26, 0x7fe51fc0, 0x1677d162
      .4byte 0x4dc520fe, 0x2457ee5c, 0x9ee0bdba, 0xf7727318
      .4byte 0xee626c87, 0x87f0a225, 0x3d47f1c3, 0x54d53f61
      .4byte 0x8a2212fb, 0xe3b0dc59, 0x59078fbf, 0x3095411d
      .4byte 0x29855e82, 0x40179020, 0xfaa0c3c6, 0x93320d64
      .4byte 0xc880fcf8, 0xa112325a, 0x1ba561bc, 0x7237af1e
      .4byte 0x6b27b081, 0x02b57e23, 0xb8022dc5, 0xd190e367
      .4byte 0x1ecf9dfa, 0x775d5358, 0xcdea00be, 0xa478ce1c
      .4byte 0xbd68d183, 0xd4fa1f21, 0x6e4d4cc7, 0x07df8265
      .4byte 0x5c6d73f9, 0x35ffbd5b, 0x8f48eebd, 0xe6da201f
      .4byte 0xffca3f80, 0x9658f122, 0x2cefa2c4, 0x457d6c66
      .4byte 0x9b8a41fc, 0xf2188f5e, 0x48afdcb8, 0x213d121a
      .4byte 0x382d0d85, 0x51bfc327, 0xeb0890c1, 0x829a5e63
      .4byte 0xd928afff, 0xb0ba615d, 0x0a0d32bb, 0x639ffc19
      .4byte 0x7a8fe386, 0x131d2d24, 0xa9aa7ec2, 0xc038b060
      .4byte 0x11a85307, 0x783a9da5, 0xc28dce43, 0xab1f00e1
      .4byte 0xb20f1f7e, 0xdb9dd1dc, 0x612a823a, 0x08b84c98
      .4byte 0x530abd04, 0x3a9873a6, 0x802f2040, 0xe9bdeee2
      .4byte 0xf0adf17d, 0x993f3fdf, 0x23886c39, 0x4a1aa29b
      .4byte 0x94ed8f01, 0xfd7f41a3, 0x47c81245, 0x2e5adce7
      .4byte 0x374ac378, 0x5ed80dda, 0xe46f5e3c, 0x8dfd909e
      .4byte 0xd64f6102, 0xbfddafa0, 0x056afc46, 0x6cf832e4
      .4byte 0x75e82d7b, 0x1c7ae3d9, 0xa6cdb03f, 0xcf5f7e9d
      .4byte 0x3d9f3bf4, 0x540df556, 0xeebaa6b0, 0x87286812
      .4byte 0x9e38778d, 0xf7aab92f, 0x4d1deac9, 0x248f246b
      .4byte 0x7f3dd5f7, 0x16af1b55, 0xac1848b3, 0xc58a8611
      .4byte 0xdc9a998e, 0xb508572c, 0x0fbf04ca, 0x662dca68
      .4byte 0xb8dae7f2, 0xd1482950, 0x6bff7ab6, 0x026db414
      .4byte 0x1b7dab8b, 0x72ef6529, 0xc85836cf, 0xa1caf86d
      .4byte 0xfa7809f1, 0x93eac753, 0x295d94b5, 0x40cf5a17
      .4byte 0x59df4588, 0x304d8b2a, 0x8afad8cc, 0xe368166e
      .4byte 0x32f8f509, 0x5b6a3bab, 0xe1dd684d, 0x884fa6ef
      .4byte 0x915fb970, 0xf8cd77d2, 0x427a2434, 0x2be8ea96
      .4byte 0x705a1b0a, 0x19c8d5a8, 0xa37f864e, 0xcaed48ec
      .4byte 0xd3fd5773, 0xba6f99d1, 0x00d8ca37, 0x694a0495
      .4byte 0xb7bd290f, 0xde2fe7ad, 0x6498b44b, 0x0d0a7ae9
      .4byte 0x141a6576, 0x7d88abd4, 0xc73ff832, 0xaead3690
      .4byte 0xf51fc70c, 0x9c8d09ae, 0x263a5a48, 0x4fa894ea
      .4byte 0x56b88b75, 0x3f2a45d7, 0x859d1631, 0xec0fd893
      .4byte 0x2350a60e, 0x4ac268ac, 0xf0753b4a, 0x99e7f5e8
      .4byte 0x80f7ea77, 0xe96524d5, 0x53d27733, 0x3a40b991
      .4byte 0x61f2480d, 0x086086af, 0xb2d7d549, 0xdb451beb
      .4byte 0xc2550474, 0xabc7cad6, 0x11709930, 0x78e25792
      .4byte 0xa6157a08, 0xcf87b4aa, 0x7530e74c, 0x1ca229ee
      .4byte 0x05b23671, 0x6c20f8d3, 0xd697ab35, 0xbf056597
      .4byte 0xe4b7940b, 0x8d255aa9, 0x3792094f, 0x5e00c7ed
      .4byte 0x4710d872, 0x2e8216d0, 0x94354536, 0xfda78b94
      .4byte 0x2c3768f3, 0x45a5a651, 0xff12f5b7, 0x96803b15
      .4byte 0x8f90248a, 0xe602ea28, 0x5cb5b9ce, 0x3527776c
      .4byte 0x6e9586f0, 0x07074852, 0xbdb01bb4, 0xd422d516
      .4byte 0xcd32ca89, 0xa4a0042b, 0x1e1757cd, 0x7785996f
      .4byte 0xa972b4f5, 0xc0e07a57, 0x7a5729b1, 0x13c5e713
      .4byte 0x0ad5f88c, 0x6347362e, 0xd9f065c8, 0xb062ab6a
      .4byte 0xebd05af6, 0x82429454, 0x38f5c7b2, 0x51670910
      .4byte 0x4877168f, 0x21e5d82d, 0x9b528bcb, 0xf2c04569
      .4byte 0x00000000, 0x7b3e77e8, 0xf67cefd0, 0x8d429838
      .4byte 0xe915a951, 0x922bdeb9, 0x1f694681, 0x64573169
      .4byte 0xd7c72453, 0xacf953bb, 0x21bbcb83, 0x5a85bc6b
      .4byte 0x3ed28d02, 0x45ecfaea, 0xc8ae62d2, 0xb390153a
      .4byte 0xaa623e57, 0xd15c49bf, 0x5c1ed187, 0x2720a66f
      .4byte 0x43779706, 0x3849e0ee, 0xb50b78d6, 0xce350f3e
      .4byte 0x7da51a04, 0x069b6dec, 0x8bd9f5d4, 0xf0e7823c
      .4byte 0x94b0b355, 0xef8ec4bd, 0x62cc5c85, 0x19f22b6d
      .4byte 0x51280a5f, 0x2a167db7, 0xa754e58f, 0xdc6a9267
      .4byte 0xb83da30e, 0xc303d4e6, 0x4e414cde, 0x357f3b36
      .4byte 0x86ef2e0c, 0xfdd159e4, 0x7093c1dc, 0x0badb634
      .4byte 0x6ffa875d, 0x14c4f0b5, 0x9986688d, 0xe2b81f65
      .4byte 0xfb4a3408, 0x807443e0, 0x0d36dbd8, 0x7608ac30
      .4byte 0x125f9d59, 0x6961eab1, 0xe4237289, 0x9f1d0561
      .4byte 0x2c8d105b, 0x57b367b3, 0xdaf1ff8b, 0xa1cf8863
      .4byte 0xc598b90a, 0xbea6cee2, 0x33e456da, 0x48da2132
      .4byte 0xa25014be, 0xd96e6356, 0x542cfb6e, 0x2f128c86
      .4byte 0x4b45bdef, 0x307bca07, 0xbd39523f, 0xc60725d7
      .4byte 0x759730ed, 0x0ea94705, 0x83ebdf3d, 0xf8d5a8d5
      .4byte 0x9c8299bc, 0xe7bcee54, 0x6afe766c, 0x11c00184
      .4byte 0x08322ae9, 0x730c5d01, 0xfe4ec539, 0x8570b2d1
      .4byte 0xe12783b8, 0x9a19f450, 0x175b6c68, 0x6c651b80
      .4byte 0xdff50eba, 0xa4cb7952, 0x2989e16a, 0x52b79682
      .4byte 0x36e0a7eb, 0x4dded003, 0xc09c483b, 0xbba23fd3
      .4byte 0xf3781ee1, 0x88466909, 0x0504f131, 0x7e3a86d9
      .4byte 0x1a6db7b0, 0x6153c058, 0xec115860, 0x972f2f88
      .4byte 0x24bf3ab2, 0x5f814d5a, 0xd2c3d562, 0xa9fda28a
      .4byte 0xcdaa93e3, 0xb694e40b, 0x3bd67c33, 0x40e80bdb
      .4byte 0x591a20b6, 0x2224575e, 0xaf66cf66, 0xd458b88e
      .4byte 0xb00f89e7, 0xcb31fe0f, 0x46736637, 0x3d4d11df
      .4byte 0x8edd04e5, 0xf5e3730d, 0x78a1eb35, 0x039f9cdd
      .4byte 0x67c8adb4, 0x1cf6da5c, 0x91b44264, 0xea8a358c
      .4byte 0x414c5f8d, 0x3a722865, 0xb730b05d, 0xcc0ec7b5
      .4byte 0xa859f6dc, 0xd3678134, 0x5e25190c, 0x251b6ee4
      .4byte 0x968b7bde, 0xedb50c36, 0x60f7940e, 0x1bc9e3e6
      .4byte 0x7f9ed28f, 0x04a0a567, 0x89e23d5f, 0xf2dc4ab7
      .4byte 0xeb2e61da, 0x90101632, 0x1d528e0a, 0x666cf9e2
      .4byte 0x023bc88b, 0x7905bf63, 0xf447275b, 0x8f7950b3
      .4byte 0x3ce94589, 0x47d73261, 0xca95aa59, 0xb1abddb1
      .4byte 0xd5fcecd8, 0xaec29b30, 0x23800308, 0x58be74e0
      .4byte 0x106455d2, 0x6b5a223a, 0xe618ba02, 0x9d26cdea
      .4byte 0xf971fc83, 0x824f8b6b, 0x0f0d1353, 0x743364bb
      .4byte 0xc7a37181, 0xbc9d0669, 0x31df9e51, 0x4ae1e9b9
      .4byte 0x2eb6d8d0, 0x5588af38, 0xd8ca3700, 0xa3f440e8
      .4byte 0xba066b85, 0xc1381c6d, 0x4c7a8455, 0x3744f3bd
      .4byte 0x5313c2d4, 0x282db53c, 0xa56f2d04, 0xde515aec
      .4byte 0x6dc14fd6, 0x16ff383e, 0x9bbda006, 0xe083d7ee
      .4byte 0x84d4e687, 0xffea916f, 0x72a80957, 0x09967ebf
      .4byte 0xe31c4b33, 0x98223cdb, 0x1560a4e3, 0x6e5ed30b
      .4byte 0x0a09e262, 0x7137958a, 0xfc750db2, 0x874b7a5a
      .4byte 0x34db6f60, 0x4fe51888, 0xc2a780b0, 0xb999f758
      .4byte 0xddcec631, 0xa6f0b1d9, 0x2bb229e1, 0x508c5e09
      .4byte 0x497e7564, 0x3240028c, 0xbf029ab4, 0xc43ced5c
      .4byte 0xa06bdc35, 0xdb55abdd, 0x561733e5, 0x2d29440d
      .4byte 0x9eb95137, 0xe58726df, 0x68c5bee7, 0x13fbc90f
      .4byte 0x77acf866, 0x0c928f8e, 0x81d017b6, 0xfaee605e
      .4byte 0xb234416c, 0xc90a3684, 0x4448aebc, 0x3f76d954
      .4byte 0x5b21e83d, 0x201f9fd5, 0xad5d07ed, 0xd6637005
      .4byte 0x65f3653f, 0x1ecd12d7, 0x938f8aef, 0xe8b1fd07
      .4byte 0x8ce6cc6e, 0xf7d8bb86, 0x7a9a23be, 0x01a45456
      .4byte 0x18567f3b, 0x636808d3, 0xee2a90eb, 0x9514e703
      .4byte 0xf143d66a, 0x8a7da182, 0x073f39ba, 0x7c014e52
      .4byte 0xcf915b68, 0xb4af2c80, 0x39edb4b8, 0x42d3c350
      .4byte 0x2684f239, 0x5dba85d1, 0xd0f81de9, 0xabc66a01
      .4byte 0x00000000, 0x8298bf1a, 0x00dd08c5, 0x8245b7df
      .4byte 0x01ba118a, 0x8322ae90, 0x0167194f, 0x83ffa655
      .4byte 0x03742314, 0x81ec9c0e, 0x03a92bd1, 0x813194cb
      .4byte 0x02ce329e, 0x80568d84, 0x02133a5b, 0x808b8541
      .4byte 0x06e84628, 0x8470f932, 0x06354eed, 0x84adf1f7
      .4byte 0x075257a2, 0x85cae8b8, 0x078f5f67, 0x8517e07d
      .4byte 0x059c653c, 0x8704da26, 0x05416df9, 0x87d9d2e3
      .4byte 0x042674b6, 0x86becbac, 0x04fb7c73, 0x8663c369
      .4byte 0x0dd08c50, 0x8f48334a, 0x0d0d8495, 0x8f953b8f
      .4byte 0x0c6a9dda, 0x8ef222c0, 0x0cb7951f, 0x8e2f2a05
      .4byte 0x0ea4af44, 0x8c3c105e, 0x0e79a781, 0x8ce1189b
      .4byte 0x0f1ebece, 0x8d8601d4, 0x0fc3b60b, 0x8d5b0911
      .4byte 0x0b38ca78, 0x89a07562, 0x0be5c2bd, 0x897d7da7
      .4byte 0x0a82dbf2, 0x881a64e8, 0x0a5fd337, 0x88c76c2d
      .4byte 0x084ce96c, 0x8ad45676, 0x0891e1a9, 0x8a095eb3
      .4byte 0x09f6f8e6, 0x8b6e47fc, 0x092bf023, 0x8bb34f39
      .4byte 0x1ba118a0, 0x9939a7ba, 0x1b7c1065, 0x99e4af7f
      .4byte 0x1a1b092a, 0x9883b630, 0x1ac601ef, 0x985ebef5
      .4byte 0x18d53bb4, 0x9a4d84ae, 0x18083371, 0x9a908c6b
      .4byte 0x196f2a3e, 0x9bf79524, 0x19b222fb, 0x9b2a9de1
      .4byte 0x1d495e88, 0x9fd1e192, 0x1d94564d, 0x9f0ce957
      .4byte 0x1cf34f02, 0x9e6bf018, 0x1c2e47c7, 0x9eb6f8dd
      .4byte 0x1e3d7d9c, 0x9ca5c286, 0x1ee07559, 0x9c78ca43
      .4byte 0x1f876c16, 0x9d1fd30c, 0x1f5a64d3, 0x9dc2dbc9
      .4byte 0x167194f0, 0x94e92bea, 0x16ac9c35, 0x9434232f
      .4byte 0x17cb857a, 0x95533a60, 0x17168dbf, 0x958e32a5
      .4byte 0x1505b7e4, 0x979d08fe, 0x15d8bf21, 0x9740003b
      .4byte 0x14bfa66e, 0x96271974, 0x1462aeab, 0x96fa11b1
      .4byte 0x1099d2d8, 0x92016dc2, 0x1044da1d, 0x92dc6507
      .4byte 0x1123c352, 0x93bb7c48, 0x11fecb97, 0x9366748d
      .4byte 0x13edf1cc, 0x91754ed6, 0x1330f909, 0x91a84613
      .4byte 0x1257e046, 0x90cf5f5c, 0x128ae883, 0x90125799
      .4byte 0x37423140, 0xb5da8e5a, 0x379f3985, 0xb507869f
      .4byte 0x36f820ca, 0xb4609fd0, 0x3625280f, 0xb4bd9715
      .4byte 0x34361254, 0xb6aead4e, 0x34eb1a91, 0xb673a58b
      .4byte 0x358c03de, 0xb714bcc4, 0x35510b1b, 0xb7c9b401
      .4byte 0x31aa7768, 0xb332c872, 0x31777fad, 0xb3efc0b7
      .4byte 0x301066e2, 0xb288d9f8, 0x30cd6e27, 0xb255d13d
      .4byte 0x32de547c, 0xb046eb66, 0x32035cb9, 0xb09be3a3
      .4byte 0x336445f6, 0xb1fcfaec, 0x33b94d33, 0xb121f229
      .4byte 0x3a92bd10, 0xb80a020a, 0x3a4fb5d5, 0xb8d70acf
      .4byte 0x3b28ac9a, 0xb9b01380, 0x3bf5a45f, 0xb96d1b45
      .4byte 0x39e69e04, 0xbb7e211e, 0x393b96c1, 0xbba329db
      .4byte 0x385c8f8e, 0xbac43094, 0x3881874b, 0xba193851
      .4byte 0x3c7afb38, 0xbee24422, 0x3ca7f3fd, 0xbe3f4ce7
      .4byte 0x3dc0eab2, 0xbf5855a8, 0x3d1de277, 0xbf855d6d
      .4byte 0x3f0ed82c, 0xbd966736, 0x3fd3d0e9, 0xbd4b6ff3
      .4byte 0x3eb4c9a6, 0xbc2c76bc, 0x3e69c163, 0xbcf17e79
      .4byte 0x2ce329e0, 0xae7b96fa, 0x2c3e2125, 0xaea69e3f
      .4byte 0x2d59386a, 0xafc18770, 0x2d8430af, 0xaf1c8fb5
      .4byte 0x2f970af4, 0xad0fb5ee, 0x2f4a0231, 0xadd2bd2b
      .4byte 0x2e2d1b7e, 0xacb5a464, 0x2ef013bb, 0xac68aca1
      .4byte 0x2a0b6fc8, 0xa893d0d2, 0x2ad6670d, 0xa84ed817
      .4byte 0x2bb17e42, 0xa929c158, 0x2b6c7687, 0xa9f4c99d
      .4byte 0x297f4cdc, 0xabe7f3c6, 0x29a24419, 0xab3afb03
      .4byte 0x28c55d56, 0xaa5de24c, 0x28185593, 0xaa80ea89
      .4byte 0x2133a5b0, 0xa3ab1aaa, 0x21eead75, 0xa376126f
      .4byte 0x2089b43a, 0xa2110b20, 0x2054bcff, 0xa2cc03e5
      .4byte 0x224786a4, 0xa0df39be, 0x229a8e61, 0xa002317b
      .4byte 0x23fd972e, 0xa1652834, 0x23209feb, 0xa1b820f1
      .4byte 0x27dbe398, 0xa5435c82, 0x2706eb5d, 0xa59e5447
      .4byte 0x2661f212, 0xa4f94d08, 0x26bcfad7, 0xa42445cd
      .4byte 0x24afc08c, 0xa6377f96, 0x2472c849, 0xa6ea7753
      .4byte 0x2515d106, 0xa78d6e1c, 0x25c8d9c3, 0xa75066d9
      .4byte 0x00000000, 0x6e846280, 0xdd08c500, 0xb38ca780
      .4byte 0xbffdfcf1, 0xd1799e71, 0x62f539f1, 0x0c715b71
      .4byte 0x7a178f13, 0x1493ed93, 0xa71f4a13, 0xc99b2893
      .4byte 0xc5ea73e2, 0xab6e1162, 0x18e2b6e2, 0x7666d462
      .4byte 0xf42f1e26, 0x9aab7ca6, 0x2927db26, 0x47a3b9a6
      .4byte 0x4bd2e2d7, 0x25568057, 0x96da27d7, 0xf85e4557
      .4byte 0x8e389135, 0xe0bcf3b5, 0x53305435, 0x3db436b5
      .4byte 0x31c56dc4, 0x5f410f44, 0xeccda8c4, 0x8249ca44
      .4byte 0xedb24abd, 0x8336283d, 0x30ba8fbd, 0x5e3eed3d
      .4byte 0x524fb64c, 0x3ccbd4cc, 0x8f47734c, 0xe1c311cc
      .4byte 0x97a5c5ae, 0xf921a72e, 0x4aad00ae, 0x2429622e
      .4byte 0x2858395f, 0x46dc5bdf, 0xf550fc5f, 0x9bd49edf
      .4byte 0x199d549b, 0x7719361b, 0xc495919b, 0xaa11f31b
      .4byte 0xa660a86a, 0xc8e4caea, 0x7b686d6a, 0x15ec0fea
      .4byte 0x638adb88, 0x0d0eb908, 0xbe821e88, 0xd0067c08
      .4byte 0xdc772779, 0xb2f345f9, 0x017fe279, 0x6ffb80f9
      .4byte 0xde88e38b, 0xb00c810b, 0x0380268b, 0x6d04440b
      .4byte 0x61751f7a, 0x0ff17dfa, 0xbc7dda7a, 0xd2f9b8fa
      .4byte 0xa49f6c98, 0xca1b0e18, 0x7997a998, 0x1713cb18
      .4byte 0x1b629069, 0x75e6f2e9, 0xc66a5569, 0xa8ee37e9
      .4byte 0x2aa7fdad, 0x44239f2d, 0xf7af38ad, 0x992b5a2d
      .4byte 0x955a015c, 0xfbde63dc, 0x4852c45c, 0x26d6a6dc
      .4byte 0x50b072be, 0x3e34103e, 0x8db8b7be, 0xe33cd53e
      .4byte 0xef4d8e4f, 0x81c9eccf, 0x32454b4f, 0x5cc129cf
      .4byte 0x333aa936, 0x5dbecbb6, 0xee326c36, 0x80b60eb6
      .4byte 0x8cc755c7, 0xe2433747, 0x51cf90c7, 0x3f4bf247
      .4byte 0x492d2625, 0x27a944a5, 0x9425e325, 0xfaa181a5
      .4byte 0xf6d0dad4, 0x9854b854, 0x2bd81fd4, 0x455c7d54
      .4byte 0xc715b710, 0xa991d590, 0x1a1d7210, 0x74991090
      .4byte 0x78e84be1, 0x166c2961, 0xa5e08ee1, 0xcb64ec61
      .4byte 0xbd023803, 0xd3865a83, 0x600afd03, 0x0e8e9f83
      .4byte 0x02ffc4f2, 0x6c7ba672, 0xdff701f2, 0xb1736372
      .4byte 0xb8fdb1e7, 0xd679d367, 0x65f574e7, 0x0b711667
      .4byte 0x07004d16, 0x69842f96, 0xda088816, 0xb48cea96
      .4byte 0xc2ea3ef4, 0xac6e5c74, 0x1fe2fbf4, 0x71669974
      .4byte 0x7d17c205, 0x1393a085, 0xa01f0705, 0xce9b6585
      .4byte 0x4cd2afc1, 0x2256cd41, 0x91da6ac1, 0xff5e0841
      .4byte 0xf32f5330, 0x9dab31b0, 0x2e279630, 0x40a3f4b0
      .4byte 0x36c520d2, 0x58414252, 0xebcde5d2, 0x85498752
      .4byte 0x8938dc23, 0xe7bcbea3, 0x54301923, 0x3ab47ba3
      .4byte 0x554ffb5a, 0x3bcb99da, 0x88473e5a, 0xe6c35cda
      .4byte 0xeab207ab, 0x8436652b, 0x37bac2ab, 0x593ea02b
      .4byte 0x2f587449, 0x41dc16c9, 0xf250b149, 0x9cd4d3c9
      .4byte 0x90a588b8, 0xfe21ea38, 0x4dad4db8, 0x23292f38
      .4byte 0xa160e57c, 0xcfe487fc, 0x7c68207c, 0x12ec42fc
      .4byte 0x1e9d198d, 0x70197b0d, 0xc395dc8d, 0xad11be0d
      .4byte 0xdb776a6f, 0xb5f308ef, 0x067faf6f, 0x68fbcdef
      .4byte 0x648a969e, 0x0a0ef41e, 0xb982539e, 0xd706311e
      .4byte 0x6675526c, 0x08f130ec, 0xbb7d976c, 0xd5f9f5ec
      .4byte 0xd988ae9d, 0xb70ccc1d, 0x04806b9d, 0x6a04091d
      .4byte 0x1c62dd7f, 0x72e6bfff, 0xc16a187f, 0xafee7aff
      .4byte 0xa39f218e, 0xcd1b430e, 0x7e97e48e, 0x1013860e
      .4byte 0x925a4c4a, 0xfcde2eca, 0x4f52894a, 0x21d6ebca
      .4byte 0x2da7b0bb, 0x4323d23b, 0xf0af75bb, 0x9e2b173b
      .4byte 0xe84dc359, 0x86c9a1d9, 0x35450659, 0x5bc164d9
      .4byte 0x57b03fa8, 0x39345d28, 0x8ab8faa8, 0xe43c9828
      .4byte 0x8bc718d1, 0xe5437a51, 0x56cfddd1, 0x384bbf51
      .4byte 0x343ae420, 0x5abe86a0, 0xe9322120, 0x87b643a0
      .4byte 0xf1d097c2, 0x9f54f542, 0x2cd852c2, 0x425c3042
      .4byte 0x4e2d6b33, 0x20a909b3, 0x9325ae33, 0xfda1ccb3
      .4byte 0x7fe806f7, 0x116c6477, 0xa2e0c3f7, 0xcc64a177
      .4byte 0xc015fa06, 0xae919886, 0x1d1d3f06, 0x73995d86
      .4byte 0x05ff89e4, 0x6b7beb64, 0xd8f74ce4, 0xb6732e64
      .4byte 0xba027515, 0xd4861795, 0x670ab015, 0x098ed295

	.size	TRI_BlockCrc32_SSE42, .-TRI_BlockCrc32_SSE42
	.size	_TRI_BlockCrc32_SSE42, .-_TRI_BlockCrc32_SSE42
/* end of TRI_BlockCrc32_SSE42  */

#if defined(__ELF__)
.section	.note.GNU-stack,"",@progbits
#endif
