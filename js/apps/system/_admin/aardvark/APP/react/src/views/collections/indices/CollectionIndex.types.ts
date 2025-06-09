import { Index, MdiIndex } from "arangojs/indexes";

type MdiPrefixed = Omit<MdiIndex, "type"> & {
  type: "mdi-prefixed";
  prefixFields: string[];
};

type Vector = {
  type: "vector";
  fields: string[];
  name?: string;
  params: {
    metric: "cosine" | "l2" | string;
    dimension: number;
    nLists: number;
    defaultNProbe?: number;
    trainingIterations?: number;
    factory?: string;
  };
};

export type CollectionIndex = Index | MdiIndex | MdiPrefixed | Vector;
