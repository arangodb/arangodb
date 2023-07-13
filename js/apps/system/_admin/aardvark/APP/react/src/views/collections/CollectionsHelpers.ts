import { CollectionStatus, CollectionType } from "arangojs/collection";

export const TYPE_TO_LABEL_MAP: {
  [key in CollectionType]: string;
} = {
  2: "Document",
  3: "Edge",
};

export const STATUS_TO_LABEL_MAP: {
  [key in CollectionStatus]: string;
} = {
  [CollectionStatus.NEWBORN]: "Newborn",
  [CollectionStatus.LOADING]: "Loading",
  [CollectionStatus.LOADED]: "Loaded",
  [CollectionStatus.UNLOADING]: "Unloading",
  [CollectionStatus.UNLOADED]: "Unloaded",
  [CollectionStatus.DELETED]: "Deleted",
};