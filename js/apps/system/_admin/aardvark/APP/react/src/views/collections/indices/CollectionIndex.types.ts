import { Index, ZkdIndex } from "arangojs/indexes";

type MdiIndex = Omit<ZkdIndex, "type"> & {
  type: "mdi";
};

type MdiPrefixed = Omit<ZkdIndex, "type"> & {
  type: "mdi-prefixed";
  prefixFields: string[];
};

export type CollectionIndex = Index | MdiIndex | MdiPrefixed;
