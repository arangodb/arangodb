import { Spinner } from "@chakra-ui/react";
import React from "react";
import { useCustomHashMatch } from "../../../utils/useCustomHashMatch";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { ArangoSearchViewSettings } from "../arangoSearchView/ArangoSearchViewSettings";
import { SearchAliasViewSettings } from "../searchAliasView/SearchAliasViewSettings";
import {
  ArangoSearchViewPropertiesType,
  SearchAliasViewPropertiesType
} from "../View.types";
import { useFetchViewProperties } from "./useFetchViewProperties";

export const SingleViewView = () => {
  const name = useCustomHashMatch("#view/");
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
