import { Spinner } from "@chakra-ui/react";
import React from "react";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { ArangoSearchViewSettings } from "../arangoSearchView/ArangoSearchViewSettings";
import { SearchAliasViewSettings } from "../searchAliasView/SearchAliasViewSettings";
import { useFetchViewProperties } from "./useFetchViewProperties";
import {
  ArangoSearchViewPropertiesType,
  SearchAliasViewPropertiesType
} from "../View.types";
import { useRouteMatch } from "react-router-dom";

export const SingleViewView = () => {
  const { params } = useRouteMatch<{ viewName: string }>();
  const { viewName: name } = params;
  const { view, isLoading } = useFetchViewProperties(name);
  useDisableNavBar();
  useGlobalStyleReset();
  if (isLoading) {
    return <Spinner />;
  }
  if (view?.type === "search-alias") {
    return (
      <SearchAliasViewSettings view={view as SearchAliasViewPropertiesType} />
    );
  }
  if (view?.type === "arangosearch") {
    return (
      <ArangoSearchViewSettings view={view as ArangoSearchViewPropertiesType} />
    );
  }
  return null;
};
