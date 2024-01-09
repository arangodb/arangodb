import React from "react";
import { CollectionIndicesViewWrap } from "./CollectionIndicesViewWrap";

const CollectionIndicesReactView = ({ collectionName, collection }) => {
  return (
    <CollectionIndicesViewWrap
      collectionName={collectionName}
      collectionId={collection.id}
    />
  );
};
window.CollectionIndicesReactView = CollectionIndicesReactView;
