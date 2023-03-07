import React from "react";
import { ViewPropertiesType } from "./useFetchViewProperties";

export const SearchAliasViewSettings = ({
  view
}: {
  view: ViewPropertiesType;
}) => {
  console.log({ view });
  return <>{view.name}</>;
};
