export type AddNewViewFormValues = {
  name: string;
  type: "arangosearch" | "search-alias";
  primarySortCompression: "lz4" | "none";
  primarySort: { field: string; direction: string }[];
  storedValues: { fields: string[]; compression: "lz4" | "none" }[];
  writebufferIdle: string;
  writebufferActive: string;
  writebufferSizeMax: string;
  indexes: { collection: string; index: string }[];
};
