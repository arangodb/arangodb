import { Index, MdiIndex } from "arangojs/indexes";

type MdiPrefixed = Omit<MdiIndex, "type"> & {
  type: "mdi-prefixed";
  prefixFields: string[];
};

export type CollectionIndex = Index | MdiIndex | MdiPrefixed;
