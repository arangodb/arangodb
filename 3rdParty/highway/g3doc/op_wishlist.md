# Potential new ops for Highway

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'janwas' reviewed: '2023-04-21' }
*-->

[TOC]

## Wishlist

### Remaining math functions for hwy/contrib/math

atan2, cbrt, cosh, erf, exp2, fmod, hypot, ilogb, lgamma, logb, modf, nextafter,
nexttoward, pow, scalbn, tan, tgamma

### Remaining STL functions for hwy/contrib/algo

*   Min/MaxValue
*   IndexOfMin/Max
*   AllOf / AnyOf / NoneOf
*   Count(If) (https://en.algorithmica.org/hpc/simd/masking/)
*   EqualSpan
*   ReverseSpan
*   ShuffleSpan
*   IsSorted
*   Reduce

### Range coder

Port https://github.com/richgel999/sserangecoding to Highway (~50 instructions).

### float64 support for WASM

### AfterN

= Not(FirstN()), replaces several instances. WHILEGE on SVE.

### 52x52=104-bit multiply

For crypto. Native on Icelake+.

### Slide1Up/Down

Potentially useful for comparing neighbors e.g. for RLE.

### RVV codegen

*   New tuple interface for segment load/store
*   Use new mask<->vec cast instruction, possibly for OddEven, ExpandLoad
*   `rgather_vx` for broadcasting redsum result?
*   Fix remaining 8-bit table lookups for large vectors (`Broadcast`,
    `Interleave`, `LoadDup128`): use 64-bit for initial shuffle. For
    `TwoTablesLookupLanes`, use 16-bit indices.

### SVE codegen

* SVE2: use XAR for `RotateRight`
* `CombineShiftRightBytes` use `TableLookupLanes` instead?
* `Shuffle*`: use `TableLookupLanes` instead?
* Use SME once available: DUP predicate, REVD (rotate 128-bit elements by 64),
  SCLAMP/UCLAMP, 128-bit TRN/UZP/ZIP (also in F64MM)

### emu128 codegen

* `#pragma unroll(1)` in all loops to enable autovectorization

### Add emu256 target
Reuse same wasm256 file, `#if` for wasm-specific parts. Use reserved avx slot.

### `MaxOfLanes, MinOfLanes` returning scalar
Avoids extra broadcast.

### Reductions for 8-bit
For orthogonality; already done for x86+NEON.

### Clear lowest mask bit
For iterating in hash table.

### Conflict detection
For hash tables. Use VPCONFLICT on ZEN4.

### `PromoteToEven`
For `WidenMul`, `MinOfLanes`.

### Add `DupEven` for 16-bit
Use in `MinOfLanes` (helps NEON).

### Masked add/sub
For tolower (subtract if in range) or hash table probing.

### Div (integer division) and Mod

Issue 633.

### `AddSub`
Interval arithmetic?

### `Dup128TableLookupBytes`
Avoids having to add offset on RVV. Table must come from `LoadDup128`.

### `LoadPromoteTo`
For SVE (svld1sb_u32)+WASM? Compiler can probably already fuse.

## Done

*   ~~Signbit~~
*   ~~ConvertF64<->I32~~ (math-inl)
*   ~~Copysign~~ (math)
*   ~~CopySignToAbs~~ (math)
*   ~~Neg~~
*   ~~Compress~~
*   ~~Mask ops~~ (math)
*   ~~RebindMask~~
*   ~~Not~~
*   ~~FP16 conversions~~
*   ~~Scatter~~
*   ~~Gather~~
*   ~~Pause~~
*   ~~Abs i64~~
*   ~~FirstN~~
*   ~~Compare i64~~
*   ~~AESRound~~
*   ~~CLMul~~ (GCM)
*   ~~TableLookupBytesOr0~~ (AES)
*   ~~FindFirstTrue~~ (strlen)
*   ~~NE~~
*   ~~Combine partial~~
*   ~~LoadMaskBits~~ (FirstN)
*   ~~MaskedLoad~~
*   ~~Bf16 promote2~~
*   ~~ConcatOdd/Even~~
*   ~~SwapAdjacentBlocks~~
*   ~~OddEvenBlocks~~
*   ~~CompressBlendedStore~~
*   ~~RotateRight~~ (Reverse2 i16)
*   ~~Compare128~~
*   ~~OrAnd~~
*   ~~IfNegativeThenElse~~
*   ~~MulFixedPoint15~~ (codec)
*   ~~Insert/ExtractLane~~
*   ~~IsNan~~
*   ~~IsFinite~~
*   ~~StoreInterleaved~~
*   ~~LoadInterleaved~~ (codec)
*   ~~Or3/Xor3~~
*   ~~NotXor~~ (sort)
*   ~~FindKnownFirstTrue~~ (sort)
*   ~~CompressStore~~ 8-bit
*   ~~ExpandLoad~~ (hash)
*   ~~Zen4 target~~ (sort)
*   ~~SSE2 target~~ - by johnplatts
*   ~~AbsDiff int~~ - by johnplatts
*   ~~Le integer~~ - by johnplatts
*   ~~LeadingZeroCount~~ - by johnplatts in #1276
*   ~~8-bit Mul~~
*   ~~(Neg)MulAdd for integer~~
*   ~~AESRoundInv etc~~ - by johnplatts in #1286
*   ~~`OddEven` for <64bit lanes: use Set of wider constant 0_1~~
*   ~~Shl for 8-bit~~
*   ~~Shr for 8-bit~~
*   ~~Faster `Reverse2` 16-bit~~
*   ~~Add `Reverse2` for 8-bit~~
*   ~~`TwoTablesLookupLanes`~~ - by johnplatts in #1303
*   ~~Add 8/16-bit `TableLookupLanes`~~ - by johnplatts in #1303
*   ~~`FindLastTrue`~~ - by johnplatts in #1308
*   ~~Vec2, Create/Get functions~~
*   ~~`PromoteTo` for all types (#915)~~ - by johnplatts in #1387
