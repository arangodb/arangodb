// translate the various posix character classes into unicode properties
// this works across all unicode locales
// { <posix class>: [<translation>, negated]
const posixClasses = {
    '[:alnum:]': ['\\p{L}\\p{Nl}\\p{Nd}'],
    '[:alpha:]': ['\\p{L}\\p{Nl}'],
    '[:ascii:]': ['\\x' + '00-\\x' + '7f'],
    '[:blank:]': ['\\p{Zs}\\t'],
    '[:cntrl:]': ['\\p{Cc}'],
    '[:digit:]': ['\\p{Nd}'],
    '[:graph:]': ['\\p{Z}\\p{C}', true],
    '[:lower:]': ['\\p{Ll}'],
    '[:print:]': ['\\p{C}', true],
    '[:punct:]': ['\\p{P}\\p{S}'],
    '[:space:]': ['\\p{Z}\\t\\r\\n\\v\\f'],
    '[:upper:]': ['\\p{Lu}'],
    '[:word:]': ['\\p{L}\\p{Nl}\\p{Nd}\\p{Pc}'],
    '[:xdigit:]': ['A-Fa-f0-9'],
};
// only need to escape a few things inside of brace expressions
// escapes: [ \ ] -
const braceEscape = (s) => s.replace(/[[\]\\-]/g, '\\$&');
const noMatch = /$./;
// Takes a glob string at a posix brace expression. Returns a RegExp
// that tests one character, and the number of glob chars consumed.
// If the class is not closed, consumed is 0, and the `[` must be
// treated as a literal character.
// Out of order ranges are removed. If nothing is left, the RegExp
// is `/$./`, and the rest of the glob is consumed, so that the
// whole glob cannot match.
export const parseClass = (glob, position, nocase = false, form) => {
    // normalize one class member, if it stays one code point
    const norm = (c) => {
        const n = form ? c.normalize(form) : c;
        return [...n].length === 1 ? n : c;
    };
    const pos = position;
    /* c8 ignore start */
    if (glob.charAt(pos) !== '[') {
        throw new Error('not in a brace expression');
    }
    /* c8 ignore stop */
    const ranges = [];
    const negs = [];
    let i = pos + 1;
    let sawStart = false;
    let escaping = false;
    let negate = false;
    let endPos = pos;
    let rangeStart = '';
    WHILE: while (i < glob.length) {
        const c = String.fromCodePoint(glob.codePointAt(i));
        if ((c === '!' || c === '^') && i === pos + 1) {
            negate = true;
            i++;
            continue;
        }
        if (c === ']' && sawStart && !escaping) {
            endPos = i + 1;
            break;
        }
        sawStart = true;
        if (c === '\\') {
            if (!escaping) {
                escaping = true;
                i++;
                continue;
            }
            // escaped \ char, fall through and treat like normal char
        }
        if (c === '[' && !escaping) {
            // either a posix class, a collation equivalent, or just a [
            for (const [cls, [unip, neg]] of Object.entries(posixClasses)) {
                if (glob.startsWith(cls, i)) {
                    // invalid, [a-[] is fine, but not [a-[:alpha]]
                    if (rangeStart) {
                        return [noMatch, glob.length - pos];
                    }
                    i += cls.length;
                    if (neg)
                        negs.push(unip);
                    else
                        ranges.push(unip);
                    continue WHILE;
                }
            }
        }
        // now it's just a normal character, effectively
        escaping = false;
        const n = norm(c);
        if (rangeStart) {
            // throw this range away if it's not valid, but others
            // can still match.
            if (n.codePointAt(0) > rangeStart.codePointAt(0)) {
                ranges.push(braceEscape(rangeStart) + '-' + braceEscape(n));
            }
            else if (n === rangeStart) {
                ranges.push(braceEscape(n));
            }
            rangeStart = '';
            i += c.length;
            continue;
        }
        // now might be the start of a range.
        // can be either c-d or c-] or c<more...>] or c] at this point
        if (glob.startsWith('-]', i + c.length)) {
            ranges.push(braceEscape(n + '-'));
            i += c.length + 1;
            continue;
        }
        if (glob.startsWith('-', i + c.length)) {
            rangeStart = n;
            i += c.length + 1;
            continue;
        }
        // not the start of a range, just a single character
        ranges.push(braceEscape(n));
        i += c.length;
    }
    if (endPos < i) {
        // didn't see the end of the class, not a valid class,
        // but might still be valid as a literal match.
        return [noMatch, 0];
    }
    // if we got no ranges and no negates, then we have a range that
    // cannot possibly match anything, and that poisons the whole glob
    if (!ranges.length && !negs.length) {
        return [noMatch, glob.length - pos];
    }
    const sranges = '[' + (negate ? '^' : '') + ranges.join('') + ']';
    const snegs = '[' + (negate ? '' : '^') + negs.join('') + ']';
    const comb = '^' +
        (ranges.length && negs.length ? '(' + sranges + '|' + snegs + ')'
            : ranges.length ? sranges
                : snegs);
    return [new RegExp(comb, nocase ? 'iu' : 'u'), endPos - pos];
};
//# sourceMappingURL=parse-class.js.map