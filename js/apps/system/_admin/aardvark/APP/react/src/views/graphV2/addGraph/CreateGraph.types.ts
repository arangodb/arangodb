import { ArangoCollection } from "arangojs/collection";
import { EdgeDefinitionOptions } from "arangojs/graph";

type GraphCreateValues = {
  name: string;
};
export type GeneralGraphCreateValues = GraphCreateValues & {
  edgeDefinitions: EdgeDefinitionOptions[];
  orphanCollections: (string | ArangoCollection)[] | string | ArangoCollection;
};
