export default [
  {
    name: "Read a document (GET)",
    type: "all",
    setup: async (ctx) => {
      const r = await ctx.request('POST', '/_db/d/_api/document/c', { value: 1 });
      return { key: r.body._key };
    },
    method: "GET",
    path: "/_db/d/_api/document/c/${ctx.data.key}",
    teardown: async (ctx) => {
      await ctx.request('DELETE', ctx.resolveString('/_db/d/_api/document/c/${ctx.data.key}'));
    },
  },

  {
    name: "Check document existence (HEAD)",
    type: "all",
    // Insert the document so there is always something to check.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "HEAD",
    path: "/_db/d/_api/document/c/testdoc",
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Insert a document with pre-specified key (POST)",
    type: "all",
    // Ensure the document does not exist before each attempt.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
    method: "POST",
    path: "/_db/d/_api/document/c",
    body: { _key: "testdoc", value: 1 },
    // Remove the document after each attempt (covers both success and failure).
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Replace a document (PUT) with key",
    type: "all",
    // Insert the document so there is always something to replace.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "PUT",
    path: "/_db/d/_api/document/c/testdoc",
    body: { value: 2 },
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Replace a document (PUT) without key",
    type: "all",
    // Insert the document so there is always something to replace.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "PUT",
    path: "/_db/d/_api/document/c",
    body: [{ _key: "testdoc", value: 2 }],
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Update a document (PATCH) with key",
    type: "all",
    // Insert the document so there is always something to patch.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "PATCH",
    path: "/_db/d/_api/document/c/testdoc",
    body: { value: 3 },
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Update a document (PATCH) without key",
    type: "all",
    // Insert the document so there is always something to patch.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "PATCH",
    path: "/_db/d/_api/document/c",
    body: [{ _key: "testdoc", value: 3 }],
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Delete a document (DELETE) with key",
    type: "all",
    // Insert the document so there is always something to delete.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "DELETE",
    path: "/_db/d/_api/document/c/testdoc",
    // Remove the document in case the DELETE was denied and it still exists.
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },

  {
    name: "Delete a document (DELETE) without key",
    type: "all",
    // Insert the document so there is always something to delete.
    setup: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
      await ctx.request('POST', '/_db/d/_api/document/c', { _key: 'testdoc', value: 1 });
    },
    method: "DELETE",
    path: "/_db/d/_api/document/c",
    body: { _key: "testdoc" },
    // Remove the document in case the DELETE was denied and it still exists.
    teardown: async (ctx) => {
      await ctx.request('DELETE', '/_db/d/_api/document/c/testdoc');
    },
  },
];
