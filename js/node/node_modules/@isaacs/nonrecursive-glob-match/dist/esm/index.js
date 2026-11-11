import { numericRange } from './numeric-range.js';
import { parseClass } from './parse-class.js';
// the end of the pattern, or of a negated sub-pattern
const MATCH = { t: 'match', id: 0 };
const extTypes = '!?+*@';
const lit = (ch) => ({ t: 'lit', ch });
const reEscape = (s) => s.replace(/[\\^$.*+?()[\]{}|/]/g, '\\$&');
// a RegExp that only matches `s`, with unicode case folding
const newFoldRe = (s) => new RegExp(`^${reEscape(s)}$`, 'iu');
// foldRe for one literal char. The same chars are in most patterns,
// so keep them. A RegExp without the `g` or `y` flag has no state to
// share.
const foldCache = new Map();
const foldRe = (ch) => {
    let re = foldCache.get(ch);
    if (!re) {
        re = newFoldRe(ch);
        if (foldCache.size >= 1000)
            foldCache.clear();
        foldCache.set(ch, re);
    }
    return re;
};
const litMatch = (l, c) => {
    if (c === l.ch)
        return true;
    if (!l.re)
        return false;
    const code = c.charCodeAt(0);
    if (code >= 128)
        return l.re.test(c);
    const ascii = (l.ascii ??= new Uint8Array(128));
    ascii[code] ||= l.re.test(c) ? 1 : 2;
    return ascii[code] === 1;
};
const tokenize = (p, nocase, noext, form, mark) => {
    const toks = [];
    for (let i = 0; i < p.length;) {
        const c = String.fromCodePoint(p.codePointAt(i));
        if (mark) {
            // a \ before a range would only escape its first char
            const m = c === '\\' && p.startsWith(mark, i + 1) ? 1 : 0;
            if (m || c === mark) {
                const start = i + m + mark.length;
                const end = p.indexOf(mark, start);
                const r = /^(-?\d+)\.\.(-?\d+)$/.exec(p.slice(start, end));
                const alts = end !== -1 && r && numericRange(r[1], r[2]);
                if (alts) {
                    toks.push({ t: 'ext', type: '@', alts });
                    i = end + mark.length;
                    continue;
                }
            }
        }
        if (c === '\\') {
            // a trailing \ is a literal \
            const e = p.codePointAt(i + 1);
            const ch = e === undefined ? c : String.fromCodePoint(e);
            toks.push(lit(ch));
            i += 1 + ch.length;
            continue;
        }
        if (c === '[') {
            const [re, consumed] = parseClass(p, i, nocase, form);
            if (consumed) {
                toks.push({ t: 'class', re });
                i += consumed;
                continue;
            }
        }
        if (!noext && extTypes.includes(c) && p[i + 1] === '(') {
            toks.push({ t: 'open', type: c });
            i += 2;
            continue;
        }
        toks.push(!noext && c === '|' ? { t: 'bar' }
            : !noext && c === ')' ? { t: 'close' }
                : c === '*' ? { t: 'star' }
                    : c === '?' ? { t: 'any' }
                        : lit(c));
        i += c.length;
    }
    return toks;
};
// map each open token index to the index of its close token
const pair = (toks) => {
    const pairs = new Map();
    const stack = [];
    toks.forEach((tok, i) => {
        if (tok.t === 'open')
            stack.push(i);
        else if (tok.t === 'close' && stack.length)
            pairs.set(stack.pop(), i);
    });
    return pairs;
};
// build the alternatives between start and end. An open token that
// is not closed is never inside a closed extglob, so bar and close
// tokens are only literal at the top level.
const build = (toks, pairs, start, end, top) => {
    let seq = [];
    const alts = [seq];
    for (let i = start; i < end; i++) {
        const tok = toks[i];
        const close = pairs.get(i);
        if (tok.t === 'open' && close !== undefined) {
            const sub = build(toks, pairs, i + 1, close, false);
            seq.push({ t: 'ext', type: tok.type, alts: sub });
            i = close;
        }
        else if (tok.t === 'bar' && !top) {
            alts.push((seq = []));
        }
        else if (tok.t === 'open') {
            seq.push(lit(tok.type), lit('('));
        }
        else if (tok.t === 'bar') {
            seq.push(lit('|'));
        }
        else if (tok.t === 'close') {
            seq.push(lit(')'));
        }
        else {
            seq.push(tok);
        }
    }
    return alts;
};
// normalize each run of literal chars as one string, so that a char
// and the combining marks after it compose (or decompose) together.
// Syntax is already parsed, so a char that normalizes to a syntax
// char (such as full-width `＊` in NFKC) stays a literal.
const normalize = (items, form) => {
    const out = [];
    let run = '';
    const flush = () => {
        for (const ch of run.normalize(form))
            out.push(lit(ch));
        run = '';
    };
    for (const it of items) {
        if (it.t === 'lit') {
            run += it.ch;
            continue;
        }
        flush();
        out.push(it.t === 'ext' ?
            { ...it, alts: it.alts.map(a => normalize(a, form)) }
            : it);
    }
    flush();
    return out;
};
// the start node, and the number of node ids. Ids are dense, from 0
// for the match node.
const compile = (items, nocase) => {
    let id = 1;
    const split = (out) => ({ t: 'split', id: id++, out });
    const seq = (items, next) => items.reduceRight((next, it) => item(it, next), next);
    const item = (it, next) => {
        switch (it.t) {
            case 'lit':
                const re = nocase ? foldRe(it.ch) : undefined;
                return { t: 'lit', id: id++, ch: it.ch, re, next };
            case 'any':
            case 'star':
                return { t: it.t, id: id++, next };
            case 'class':
                return { t: 'class', id: id++, re: it.re, next };
        }
        const alts = (next) => it.alts.map(a => seq(a, next));
        switch (it.type) {
            case '@':
                return split(alts(next));
            case '?':
                return split([next, ...alts(next)]);
            case '!':
                return { t: 'neg', id: id++, sub: split(alts(MATCH)), next };
        }
        // * and +, loop back after each match
        const after = split([]);
        const entry = split(alts(after));
        after.out.push(next, entry);
        return it.type === '*' ? after : entry;
    };
    const start = seq(items, MATCH);
    return [start, id];
};
// one line per node, in the order that a walk from the start
// reaches them. Edges point to node ids. The nodes of a negated
// sub-pattern are indented under their negation.
const show = (start) => {
    const lines = [];
    const seen = new Set();
    const ref = (n) => (n.t === 'match' ? 'match' : String(n.id));
    const walk = (n, indent) => {
        if (n.t === 'match' || seen.has(n))
            return;
        seen.add(n);
        const line = (desc) => lines.push(`${indent}${n.id}: ${desc}`);
        switch (n.t) {
            case 'split':
                line(`split -> ${n.out.map(ref).join(', ')}`);
                for (const o of n.out)
                    walk(o, indent);
                return;
            case 'neg':
                line(`!(${ref(n.sub)}) -> ${ref(n.next)}`);
                walk(n.sub, indent + '  ');
                return walk(n.next, indent);
            case 'lit':
                line(`${JSON.stringify(n.ch)} -> ${ref(n.next)}`);
                break;
            case 'class':
                line(`${n.re} -> ${ref(n.next)}`);
                break;
            default:
                line(`${n.t === 'any' ? '?' : '*'} -> ${ref(n.next)}`);
        }
        walk(n.next, indent);
    };
    walk(start, '');
    return [`start -> ${ref(start)}`, ...lines].join('\n');
};
// The threads at one position in the string. Each node thread is
// only kept once, and runs with the same state are only kept once,
// so that identical threads merge. The match node is not in
// `threads`, because it can not step. It sets `match` instead.
class State {
    // set by the Sim, to mark the nodes that this state has
    gen;
    threads = [];
    match = false;
    runs = [];
    // the sorted ids of the threads, with 0 for a match, once the
    // state is done
    ids = [];
    constructor(gen) {
        this.gen = gen;
    }
    // no threads, so nothing more can match
    get dead() {
        return !this.threads.length && !this.match && !this.runs.length;
    }
}
// true if the two states hold the same threads
const sameState = (a, b) => {
    if (a.ids.length !== b.ids.length)
        return false;
    if (a.runs.length !== b.runs.length)
        return false;
    if (a.ids.some((id, i) => id !== b.ids[i]))
        return false;
    return a.runs.every(r => b.runs.some(o => o.neg === r.neg && sameState(o.state, r.state)));
};
const marks = (size) => ({
    has: new Float64Array(size),
    seen: new Float64Array(size),
    gen: 0,
});
class Sim {
    // `.` and `..` only match literal dots, even with `dot:true`
    dots;
    // the string starts with `.`, and `dot:true` is not set
    dotStart;
    #marks;
    constructor(dots, dotStart, marks) {
        this.dots = dots;
        this.dotStart = dotStart;
        this.#marks = marks;
    }
    state() {
        return new State(++this.#marks.gen);
    }
    // sort the ids of a state, once it has all of its threads
    done(state) {
        state.ids = state.threads.map(n => n.id).sort((a, b) => a - b);
        if (state.match)
            state.ids.unshift(MATCH.id);
        return state;
    }
    // true the first time that the state walks through the node, so
    // that loops in the graph stop
    #visit(state, node) {
        const { seen } = this.#marks;
        if (seen[node.id] === state.gen)
            return false;
        seen[node.id] = state.gen;
        return true;
    }
    #addThread(state, node) {
        const { has } = this.#marks;
        if (has[node.id] === state.gen)
            return false;
        has[node.id] = state.gen;
        state.threads.push(node);
        return true;
    }
    // wildcards only match a leading dot if `dot:true`. In a negated
    // sub-pattern, this does not apply, because the negation itself
    // can not match a leading dot.
    restricted(i, top) {
        return top && (this.dots || (this.dotStart && i === 0));
    }
    add(state, node, i, top) {
        switch (node.t) {
            case 'split':
                if (!this.#visit(state, node))
                    return;
                for (const o of node.out)
                    this.add(state, o, i, top);
                return;
            case 'neg':
                if (!this.#visit(state, node))
                    return;
                const sub = this.state();
                this.add(sub, node.sub, i, false);
                this.done(sub);
                return this.addRun(state, { t: 'run', neg: node, state: sub }, i, top);
            case 'star':
                // a * can not start a dotfile, even if it matches nothing
                if (this.restricted(i, top))
                    return;
                if (this.#addThread(state, node))
                    this.add(state, node.next, i, top);
                return;
            case 'match':
                state.match = true;
                return;
            default:
                this.#addThread(state, node);
        }
    }
    addRun(state, run, i, top) {
        for (const r of state.runs) {
            if (r.neg === run.neg && sameState(r.state, run.state))
                return;
        }
        state.runs.push(run);
        if (!run.state.match) {
            this.add(state, run.neg.next, i, top);
        }
    }
    // the threads after char `c`, at index `i`
    step(state, c, i, top) {
        const next = this.state();
        for (const th of state.threads) {
            switch (th.t) {
                case 'lit':
                    if (litMatch(th, c)) {
                        this.add(next, th.next, i + 1, top);
                    }
                    break;
                case 'any':
                    if (!this.restricted(i, top))
                        this.add(next, th.next, i + 1, top);
                    break;
                case 'class':
                    if (!this.restricted(i, top) && th.re.test(c)) {
                        this.add(next, th.next, i + 1, top);
                    }
                    break;
                case 'star':
                    if (!this.restricted(i, top))
                        this.add(next, th, i + 1, top);
            }
        }
        if (!this.restricted(i, top)) {
            for (const run of state.runs) {
                // a sub-pattern with no threads can not match again, so the
                // run stays the same
                if (run.state.dead) {
                    this.addRun(next, run, i + 1, top);
                    continue;
                }
                const sub = this.step(run.state, c, i, false);
                this.addRun(next, { t: 'run', neg: run.neg, state: sub }, i + 1, top);
            }
        }
        return this.done(next);
    }
}
// A canonical key for the threads in a state. States with the same
// key hold the same threads, so they match the same strings.
const stateKey = (state) => {
    const ids = state.ids.join();
    if (!state.runs.length)
        return ids;
    const runs = state.runs.map(r => `${r.neg.id}(${stateKey(r.state)})`);
    return `${ids}|${runs.sort().join('|')}`;
};
// The cache can hold about this many chars of state keys and
// transitions. When it is full, it is cleared, so that a pattern
// can not use an unbounded amount of memory.
const DFA_BUDGET = 1 << 18;
// A cache of the states that the simulation reaches, built as the
// matcher is used. Each char of a string is then one Map lookup,
// once the states that it leads to are known.
//
// The dot rules only restrict the first char of a dotfile (strings
// that are only `.` or `..` do not use the cache). So a dotfile
// starts at its own state, which takes its first step with the
// restriction, and every other state takes steps without it.
class Dfa {
    #start;
    #states = new Map();
    #size = 0;
    #plain;
    #dotStart;
    #sim;
    #dotSim;
    constructor(start, marks) {
        this.#start = start;
        this.#sim = new Sim(false, false, marks);
        this.#dotSim = new Sim(false, true, marks);
    }
    #make(state) {
        return {
            state,
            match: state.match,
            dead: state.dead,
            next: new Map(),
        };
    }
    #intern(state) {
        const key = stateKey(state);
        const found = this.#states.get(key);
        if (found)
            return found;
        if (this.#size > DFA_BUDGET)
            this.#clear();
        const d = this.#make(state);
        this.#states.set(key, d);
        this.#size += key.length + 1;
        return d;
    }
    #clear() {
        this.#states.clear();
        this.#size = 0;
        this.#plain = undefined;
        this.#dotStart = undefined;
    }
    start(dotStart) {
        if (dotStart) {
            if (this.#dotStart)
                return this.#dotStart;
            const state = this.#dotSim.state();
            this.#dotSim.add(state, this.#start, 0, true);
            return (this.#dotStart = this.#make(this.#dotSim.done(state)));
        }
        if (this.#plain)
            return this.#plain;
        const state = this.#sim.state();
        this.#sim.add(state, this.#start, 0, true);
        return (this.#plain = this.#intern(this.#sim.done(state)));
    }
    // the next state, after a miss in `d.ascii`
    next(d, c) {
        const code = c.charCodeAt(0);
        const found = code < 128 ? undefined : d.next.get(c);
        if (found)
            return found;
        // the dotfile start is not in the cache, so it is never shared
        const n = d === this.#dotStart ?
            this.#intern(this.#dotSim.step(d.state, c, 0, true))
            : this.#intern(this.#sim.step(d.state, c, 1, true));
        if (code < 128) {
            if (!d.ascii) {
                d.ascii = new Array(128);
                this.#size += 128;
            }
            d.ascii[code] = n;
        }
        else {
            d.next.set(c, n);
        }
        this.#size++;
        return n;
    }
}
// the nodes in order, if the graph is a single chain
const linear = (start) => {
    const nodes = [];
    for (let n = start; n.t !== 'match'; n = n.next) {
        if (n.t === 'split' || n.t === 'neg')
            return undefined;
        nodes.push(n);
    }
    return nodes;
};
// Match a chain of nodes. Each node other than `*` matches exactly
// one char, so on a failure it is enough to go back to the last `*`
// and let it match one more char. This takes at most
// nodes.length * chars.length steps.
const testLinear = (nodes, chars, start, end) => {
    let ni = 0;
    let ci = start;
    let starNi = -1;
    let starCi = 0;
    while (ci < end) {
        const n = nodes[ni];
        if (n?.t === 'star') {
            starNi = ni++;
            starCi = ci;
            continue;
        }
        const c = chars[ci];
        if (n &&
            (n.t === 'any' ||
                (n.t === 'lit' && litMatch(n, c)) ||
                (n.t === 'class' && n.re.test(c)))) {
            ni++;
            ci++;
            continue;
        }
        if (starNi === -1)
            return false;
        ni = starNi + 1;
        ci = ++starCi;
    }
    while (nodes[ni]?.t === 'star')
        ni++;
    return ni === nodes.length;
};
// The least and most chars that the items can match. These are only
// bounds: a negation can match a string of any length, so it counts
// as 0 to Infinity.
const lengths = (items) => {
    let min = 0;
    let max = 0;
    for (const it of items) {
        const [lo, hi] = itemLengths(it);
        min += lo;
        max += hi;
    }
    return [min, max];
};
const itemLengths = (it) => {
    if (it.t === 'star')
        return [0, Infinity];
    if (it.t !== 'ext')
        return [1, 1];
    if (it.type === '!')
        return [0, Infinity];
    const alts = it.alts.map(lengths);
    const min = Math.min(...alts.map(a => a[0]));
    const max = Math.max(...alts.map(a => a[1]));
    // a loop of alternatives that only match the empty string is empty
    const loop = max === 0 ? 0 : Infinity;
    return (it.type === '@' ? [min, max]
        : it.type === '?' ? [0, max]
            : it.type === '*' ? [0, loop]
                : [min, loop]);
};
// true if the chars from `start` match the literals
const hasAffix = (lits, chars, start) => {
    for (let j = 0; j < lits.length; j++) {
        if (!litMatch(lits[j], chars[start + j]))
            return false;
    }
    return true;
};
// a `/`, or half of a surrogate pair
const slashOrSurrogate = /[/\ud800-\udfff]/;
const surrogate = /[\ud800-\udfff]/;
// a string can be indexed by code point, unless it has surrogate
// pairs
const codePoints = (s) => surrogate.test(s) ? [...s] : s;
const noSlash = (s) => {
    if (s.includes('/')) {
        throw new Error('can only match a single path portion, got: ' + s);
    }
};
export class Matcher {
    pattern;
    options;
    #start;
    // the number of node ids
    #size;
    #marks;
    // the Sim for strings that are only `.` or `..`
    #dotsSim;
    // the string to compare, if the pattern has no magic. With
    // `nocase:true`, the RegExp that folds case is made when it is
    // first used.
    #literal;
    #literalFold = false;
    #literalRe;
    // the nodes between the prefix and suffix, if the graph is a
    // single chain
    #linear;
    // the linear nodes are only `*` and `?`, so the length bounds and
    // the prefix and suffix decide the match
    #wild = false;
    // the least and most chars that a match can have
    #min = 0;
    #max = Infinity;
    // the literals that a match must start and end with. Each one
    // matches exactly one char, and they are distinct items in the
    // pattern, so a string that does not have them can not match.
    #prefix = [];
    #suffix = [];
    // the prefix and suffix as strings, if they do not fold case
    #prefixStr;
    #suffixStr;
    // there is a prefix or suffix to check
    #affix = false;
    #dfa;
    #form;
    constructor(pattern, options = {}) {
        if (pattern.includes('/')) {
            throw new Error('can only match a single path portion, got pattern: ' + pattern);
        }
        this.pattern = pattern;
        this.options = options;
        const { nocase = false, noext = false, normalize: n } = options;
        this.#form = n === true ? 'NFC' : n || undefined;
        // a pattern that is only an empty extglob is a literal string,
        // except for `!()`, which matches any non-empty string
        const literal = /^[?*+@]\(\)$/.test(pattern);
        const toks = tokenize(pattern, nocase, noext || literal, this.#form, options.rangeMark);
        let [items] = build(toks, pair(toks), 0, toks.length, true);
        if (this.#form)
            items = normalize(items, this.#form);
        const [start, size] = compile(items, nocase);
        this.#start = start;
        this.#size = size;
        if (items.every(it => it.t === 'lit')) {
            this.#literal = items.map(it => it.ch).join('');
            this.#literalFold = nocase;
        }
        else {
            const lit = (it) => it.t !== 'lit' ? undefined
                : nocase ? { ch: it.ch, re: foldRe(it.ch) }
                    : { ch: it.ch };
            for (const it of items) {
                const l = lit(it);
                if (!l)
                    break;
                this.#prefix.push(l);
            }
            for (let j = items.length - 1; j >= 0; j--) {
                const l = lit(items[j]);
                if (!l)
                    break;
                this.#suffix.unshift(l);
            }
            this.#affix = !!(this.#prefix.length || this.#suffix.length);
            if (!nocase) {
                this.#prefixStr = this.#prefix.map(l => l.ch).join('');
                this.#suffixStr = this.#suffix.map(l => l.ch).join('');
            }
            this.#linear = linear(this.#start)?.slice(this.#prefix.length, -this.#suffix.length || undefined);
            this.#wild = !!this.#linear?.every(n => n.t === 'star' || n.t === 'any');
            const [min, max] = lengths(items);
            this.#min = min;
            this.#max = max;
        }
    }
    test(s) {
        // The `/` check uses the string as given, so with normalization it
        // is a separate step. Otherwise, one scan finds both a `/` and the
        // surrogate pairs that stop the string being indexed by code point.
        if (this.#form) {
            noSlash(s);
            return this.#test(s.normalize(this.#form));
        }
        if (slashOrSurrogate.test(s)) {
            noSlash(s);
            return this.#test(s, [...s]);
        }
        return this.#test(s, s);
    }
    /**
     * The same as {@link Matcher#test}, but the string is not checked
     * for `/` chars, so the result for a string that has one is not
     * defined. Only use it for a string that is known to be one path
     * portion, such as a part of a path that was split on `/`. It can
     * be faster, because some patterns do not need to look at each char
     * of the string.
     */
    testUnchecked(s) {
        return this.#test(this.#form ? s.normalize(this.#form) : s);
    }
    // Test a string that has no `/`. `chars` indexes it by code point.
    // If it is not given, it is only found if a step needs it.
    #test(s, chars) {
        if (this.#literal !== undefined) {
            return this.#literalFold ?
                (this.#literalRe ??= newFoldRe(this.#literal)).test(s)
                : s === this.#literal;
        }
        // the length check means that the prefix and suffix do not
        // overlap. Without the chars, the length in code points is from
        // half of the UTF-16 length (if each one is a surrogate pair) to
        // all of it, which is often enough to know.
        const min = this.#min;
        const max = this.#max;
        if (chars === undefined) {
            const most = s.length;
            const least = (most + 1) >> 1;
            if (most < min || least > max)
                return false;
            if (least < min || most > max)
                chars = codePoints(s);
        }
        if (chars !== undefined &&
            (chars.length < min || chars.length > max)) {
            return false;
        }
        const prefix = this.#prefix.length;
        if (this.#affix) {
            if (this.#prefixStr !== undefined && this.#suffixStr !== undefined) {
                if (!s.startsWith(this.#prefixStr) ||
                    !s.endsWith(this.#suffixStr)) {
                    return false;
                }
            }
            else {
                chars ??= codePoints(s);
                const end = chars.length - this.#suffix.length;
                if (!hasAffix(this.#prefix, chars, 0) ||
                    !hasAffix(this.#suffix, chars, end)) {
                    return false;
                }
            }
        }
        const dotStart = !this.options.dot && s.startsWith('.');
        if (this.#linear) {
            // `.` and `..` only match literal dots, even with `dot:true`
            if (s === '.' || s === '..')
                return false;
            // only a literal can match the leading dot. A `*` can not
            // start a dotfile, even if it matches nothing.
            if (dotStart && !prefix)
                return false;
            if (this.#wild)
                return true;
            chars ??= codePoints(s);
            const end = chars.length - this.#suffix.length;
            return testLinear(this.#linear, chars, prefix, end);
        }
        chars ??= codePoints(s);
        if (s === '.' || s === '..') {
            this.#marks ??= marks(this.#size);
            const sim = (this.#dotsSim ??= new Sim(true, false, this.#marks));
            const start = sim.state();
            sim.add(start, this.#start, 0, true);
            let state = sim.done(start);
            for (let i = 0; i < chars.length; i++) {
                state = sim.step(state, chars[i], i, true);
            }
            return state.match;
        }
        this.#marks ??= marks(this.#size);
        const dfa = (this.#dfa ??= new Dfa(this.#start, this.#marks));
        let d = dfa.start(dotStart);
        // read char codes from the string, so that a one-char string is
        // only made for a cache miss
        const str = chars === s;
        for (let i = 0; i < chars.length; i++) {
            if (d.dead)
                return false;
            const code = str ? s.charCodeAt(i) : chars[i].charCodeAt(0);
            d = d.ascii?.[code] ?? dfa.next(d, chars[i]);
        }
        return d.match;
    }
    /** a readable listing of the state machine */
    toString() {
        return show(this.#start);
    }
}
//# sourceMappingURL=index.js.map