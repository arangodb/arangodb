import { CreateGraphOptions } from "arangojs/graph";

type FormEdgeDefinitionOptions = {
  collection: string;
  from: string[];
  to: string[];
};

type GraphCreateValues = {
  name: string;
  edgeDefinitions: FormEdgeDefinitionOptions[];
  orphanCollections?: string[];
};

export type GeneralGraphCreateValues = GraphCreateValues;
export type SatelliteGraphCreateValues = GraphCreateValues &
  Pick<CreateGraphOptions, "replicationFactor">;
export type EnterpriseGraphCreateValues = GraphCreateValues &
  Pick<
    CreateGraphOptions,
    | "isSmart"
    | "numberOfShards"
    | "replicationFactor"
    | "writeConcern"
    | "satellites"
  >;
export type SmartGraphCreateValues = GraphCreateValues &
  Pick<
    CreateGraphOptions,
    | "isSmart"
    | "numberOfShards"
    | "replicationFactor"
    | "writeConcern"
    | "isDisjoint"
    | "smartGraphAttribute"
    | "satellites"
  >;
