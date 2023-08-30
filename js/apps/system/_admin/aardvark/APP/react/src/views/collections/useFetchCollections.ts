import { CollectionMetadata } from "arangojs/collection";
import { useEffect, useState } from "react";
import useSWR from "swr";
import {  getCurrentDB } from "../../utils/arangoClient";

export interface LockableCollectionDescription extends CollectionMetadata {
  isLocked?: boolean;
  lockReason?: string;
}

export const useFetchCollections = () => {
  const { data, ...rest } = useSWR<LockableCollectionDescription[]>("/collections", () => {
    return getCurrentDB().listCollections(false);
  });

  const [collections, setCollections] = useState<LockableCollectionDescription[] | undefined>(
    data
  );

  const checkProgress = () => {
    const callback = (_: any, lockedCollections?: { collection: string; desc: string; }[]) => {
      const newCollections = data?.map(collection => {
        const lock = lockedCollections?.find(lockedCollection => lockedCollection.collection === collection.name);
        if (
          lock
        ) {
          return { ...collection, isLocked: true, lockReason:  lock.desc};
        }
        return { ...collection, isLocked: false };
      });
      if (newCollections) {
        setCollections(newCollections);
      }
    };
    if (!window.frontendConfig.ldapEnabled) {
      window.arangoHelper.syncAndReturnUnfinishedAardvarkJobs("index", callback);
    }
  };

  useEffect(() => {
    let interval: number;
    if (data) {
      checkProgress();
      interval = window.setInterval(() => {
        checkProgress();
      }, 10000);
    }
    return () => window.clearInterval(interval);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [data]);

  useEffect(() => {
    setCollections(data);
  }, [data]);
  return { collections, ...rest };
};