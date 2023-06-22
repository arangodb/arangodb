import _ from "lodash";

export const parseQuery = (query: string) => {
  const STATE_NORMAL = 0;
  const STATE_STRING_SINGLE = 1;
  const STATE_STRING_DOUBLE = 2;
  const STATE_STRING_TICK = 3;
  const STATE_COMMENT_SINGLE = 4;
  const STATE_COMMENT_MULTI = 5;
  const STATE_BIND = 6;
  const STATE_STRING_BACKTICK = 7;
  const bindParamRegExp = /@(@?\w+\d*)/;

  query += " ";
  let start;
  let state = STATE_NORMAL;
  const n = query.length;
  let i;
  let c;

  var bindParams = [];

  for (i = 0; i < n; ++i) {
    c = query.charAt(i);

    switch (state) {
      case STATE_NORMAL:
        if (c === "@") {
          state = STATE_BIND;
          start = i;
        } else if (c === "'") {
          state = STATE_STRING_SINGLE;
        } else if (c === '"') {
          state = STATE_STRING_DOUBLE;
        } else if (c === "`") {
          state = STATE_STRING_TICK;
        } else if (c === "´") {
          state = STATE_STRING_BACKTICK;
        } else if (c === "/") {
          if (i + 1 < n) {
            if (query.charAt(i + 1) === "/") {
              state = STATE_COMMENT_SINGLE;
              ++i;
            } else if (query.charAt(i + 1) === "*") {
              state = STATE_COMMENT_MULTI;
              ++i;
            }
          }
        }
        break;
      case STATE_COMMENT_SINGLE:
        if (c === "\r" || c === "\n") {
          state = STATE_NORMAL;
        }
        break;
      case STATE_COMMENT_MULTI:
        if (c === "*") {
          if (i + 1 <= n && query.charAt(i + 1) === "/") {
            state = STATE_NORMAL;
            ++i;
          }
        }
        break;
      case STATE_STRING_SINGLE:
        if (c === "\\") {
          ++i;
        } else if (c === "'") {
          state = STATE_NORMAL;
        }
        break;
      case STATE_STRING_DOUBLE:
        if (c === "\\") {
          ++i;
        } else if (c === '"') {
          state = STATE_NORMAL;
        }
        break;
      case STATE_STRING_TICK:
        if (c === "`") {
          state = STATE_NORMAL;
        }
        break;
      case STATE_STRING_BACKTICK:
        if (c === "´") {
          state = STATE_NORMAL;
        }
        break;
      case STATE_BIND:
        if (!/^[@a-zA-Z0-9_]+$/.test(c)) {
          bindParams.push(query.substring(start as number, i));
          state = STATE_NORMAL;
          start = undefined;
        }
        break;
    }
  }

  let match;
  _.each(bindParams, function (v, k) {
    match = v.match(bindParamRegExp);

    if (match) {
      bindParams[k] = match[1];
    }
  });

  return {
    query: query,
    bindParams: bindParams
  };
};
