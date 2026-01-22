import { Index, MdiIndex } from "arangojs/indexes";

type MdiPrefixed = Omit<MdiIndex, "type"> & {
  type: "mdi-prefixed";
  prefixFields: string[];
};

type Vector = {
  type: "vector";
  fields: string[];
  storedValues?: string[];
  name?: string;
  params: {
    metric: "cosine" | "l2" | "innerProduct" | string;
    dimension: number;
    nLists: number;
    defaultNProbe?: number;
    trainingIterations?: number;
    factory?: string;
  };
  parallelism: number;
  inBackground: boolean;
};

export type CollectionIndex = Index | MdiIndex | MdiPrefixed | Vector;
