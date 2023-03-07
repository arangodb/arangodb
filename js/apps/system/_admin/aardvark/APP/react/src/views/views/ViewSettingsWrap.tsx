import { Spinner } from "@chakra-ui/react";
import React from "react";
import { ViewSettings } from "./ViewSettings";
import { SearchAliasViewSettings } from "./viewSettings/SearchAliasViewSettings";
import { useFetchViewProperties } from "./viewSettings/useFetchViewProperties";

export const ViewSettingsWrap = ({
  name,
  isCluster
}: {
  name: string;
  isCluster: boolean;
}) => {
  const { view, isLoading } = useFetchViewProperties(name);
  if (isLoading) {
    return <Spinner />;
  }
  if(view?.type === "search-alias") {
     return <SearchAliasViewSettings view={view} />
  }
  return <ViewSettings name={name} isCluster={isCluster} />;
};
