import React from "react";
import { CollectionIndicesProvider } from "./CollectionIndicesContext";
import { CollectionIndicesTab } from "./CollectionIndicesTab";

export const CollectionIndicesTabWrap = ({
  collectionName,
  collection
}: {
  collectionName: string;
  collection: { id: string; attributes: { id: string } };
}) => {
  return (
    <CollectionIndicesProvider
      collectionName={collectionName}
      collection={collection}
    >
      <CollectionIndicesTab />
    </CollectionIndicesProvider>
  );
};
