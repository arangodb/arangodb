import { useEffect } from "react";

export const useSetupNav = ({ collectionName }: { collectionName: string }) => {
  useEffect(
    () => window.arangoHelper.buildCollectionSubNav(collectionName, "Indexes"),
    [collectionName]
  );
};
