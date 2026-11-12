// One digit, or a class of digits from a to b.
const digit = (a, b) => a === b ?
    { t: 'lit', ch: String(a) }
    : { t: 'class', re: new RegExp(`^[${a}-${b}]$`) };
const prefix = (d, alts) => alts.map(s => [digit(d, d), ...s]);
// The digit strings from a to b, which have the same length.
const fixed = (a, b) => {
    if (!a)
        return [[]];
    let lo = +a[0];
    let hi = +b[0];
    const ra = a.slice(1);
    const rb = b.slice(1);
    if (lo === hi)
        return prefix(lo, fixed(ra, rb));
    const zeros = '0'.repeat(ra.length);
    const nines = '9'.repeat(ra.length);
    const out = [];
    if (ra !== zeros)
        out.push(...prefix(lo++, fixed(ra, nines)));
    const last = rb === nines ? [] : prefix(hi--, fixed(zeros, rb));
    if (lo <= hi) {
        out.push([digit(lo, hi), ...Array.from(ra, () => digit(0, 9))]);
    }
    out.push(...last);
    return out;
};
// The numbers from a to b, where 0 <= a <= b. If width is set, each
// one has leading zeros to make it that many digits.
const nonneg = (a, b, width) => {
    if (width) {
        return fixed(String(a).padStart(width, '0'), String(b).padStart(width, '0'));
    }
    const out = [];
    for (let len = String(a).length; len <= String(b).length; len++) {
        const lo = Math.max(a, len === 1 ? 0 : 10 ** (len - 1));
        const hi = Math.min(b, 10 ** len - 1);
        out.push(...fixed(String(lo), String(hi)));
    }
    return out;
};
/**
 * The strings that the bash brace expansion `{lo..hi}` makes, as
 * the alternatives of an `@(...)` extglob. Each one is a sequence of
 * digits and digit classes, so there are only a few of them, even for
 * a large range.
 *
 * If either end has a leading zero, such as `{01..10}`, each number
 * has leading zeros to make it as long as the longer end, counting a
 * `-` sign. So `{-05..3}` makes `-05` to `-01`, then `000` to `003`.
 *
 * Returns undefined if either end is not a safe integer.
 */
export const numericRange = (loStr, hiStr) => {
    let lo = Number(loStr);
    let hi = Number(hiStr);
    if (!Number.isSafeInteger(lo) || !Number.isSafeInteger(hi))
        return;
    if (lo > hi)
        [lo, hi] = [hi, lo];
    const padded = /^-?0\d/.test(loStr) || /^-?0\d/.test(hiStr);
    const width = padded ? Math.max(loStr.length, hiStr.length) : 0;
    const out = [];
    if (lo < 0) {
        const neg = nonneg(Math.max(-hi, 1), -lo, width && width - 1);
        out.push(...neg.map(s => [{ t: 'lit', ch: '-' }, ...s]));
    }
    if (hi >= 0)
        out.push(...nonneg(Math.max(lo, 0), hi, width));
    return out;
};
//# sourceMappingURL=numeric-range.js.map