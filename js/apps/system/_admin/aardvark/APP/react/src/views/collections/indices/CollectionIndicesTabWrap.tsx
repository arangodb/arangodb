import React from "react";
import { CollectionIndicesProvider } from "./CollectionIndicesContext";
import { CollectionIndicesTab } from "./CollectionIndicesTab";

export const CollectionIndicesTabWrap = ({
  collectionName
}: {
  collectionName: string;
  collection: any;
}) => {
  return (
    <CollectionIndicesProvider collectionName={collectionName}>
      <CollectionIndicesTab />
    </CollectionIndicesProvider>
  );
};
