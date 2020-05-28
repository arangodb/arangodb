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

/* TRI_BlockCrc32_SSE42 assembler */
/* assumes presence of SSE4.2     */
/*  %rax     uint32_t             */
/*       TRI_BlockCrc32_SSE42     */
/*  %rdi     uint32_t             */
/*  %rsi     char const* data     */
/*  %rdx     size_t length        */

	.text
	.globl	TRI_BlockCrc32_SSE42
	.globl	_TRI_BlockCrc32_SSE42
#ifndef __APPLE__
	.type	TRI_BlockCrc32_SSE42, @function
	.type	_TRI_BlockCrc32_SSE42, @function
#endif
TRI_BlockCrc32_SSE42:
_TRI_BlockCrc32_SSE42:
        movl    %edi,%eax
crca1:
        cmpq    $8,%rdx
        jb      crca4
        movq    (%rsi),%rcx
        crc32   %rcx,%rax
        subq    $8,%rdx
        addq    $8,%rsi
        jmp     crca1
crca4:
        cmpq    $0,%rdx
        je      crca9
        movb    (%rsi),%cl
        crc32   %cl,%eax
        subq    $1,%rdx
        addq    $1,%rsi
        jmp     crca4
crca9:
  ret

#ifndef __APPLE__
	.size	TRI_BlockCrc32_SSE42, .-TRI_BlockCrc32_SSE42
	.size	_TRI_BlockCrc32_SSE42, .-_TRI_BlockCrc32_SSE42
#endif
/* end of TRI_BlockCrc32_SSE42  */

#if defined(__ELF__)
.section	.note.GNU-stack,"",@progbits
#endif
