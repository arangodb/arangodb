'use strict';

module.exports = {
  'canonical':        new Date(Date.UTC(2001, 11, 15, 2, 59, 43, 100)),
  'valid iso8601':    new Date(Date.UTC(2001, 11, 15, 2, 59, 43, 100)),
  'space separated':  new Date(Date.UTC(2001, 11, 15, 2, 59, 43, 100)),
  'no time zone (Z)': new Date(Date.UTC(2001, 11, 15, 2, 59, 43, 100)),
  'date (00:00:00Z)': new Date(Date.UTC(2002, 11, 14))
};
