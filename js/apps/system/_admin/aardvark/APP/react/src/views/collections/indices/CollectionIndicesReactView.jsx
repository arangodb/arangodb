import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { CollectionIndicesTab } from "./CollectionIndicesTab";

const CollectionIndicesReactView = ({ collectionName, collection }) => {
  return (
    <ChakraCustomProvider>
      <CollectionIndicesTab
        collection={collection}
        collectionName={collectionName}
      />
    </ChakraCustomProvider>
  );
};
window.CollectionIndicesReactView = CollectionIndicesReactView;
