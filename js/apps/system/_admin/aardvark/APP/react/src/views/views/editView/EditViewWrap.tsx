import { Spinner } from "@chakra-ui/react";
import React from "react";
import { ChakraCustomProvider } from "../../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../../utils/useGlobalStyleReset";
import { ArangoSearchViewSettings } from "../arangoSearchView/ArangoSearchViewSettings";
import { SearchAliasViewSettings } from "../searchAliasView/SearchAliasViewSettings";
import { useFetchViewProperties } from "./useFetchViewProperties";
import {
  ArangoSearchViewPropertiesType,
  SearchAliasViewPropertiesType
} from "../searchView.types";
import { SearchViewsCustomStyleReset } from "../SearchViewsCustomStyleReset";

export const EditViewWrap = ({ name }: { name: string }) => {
  const { view, isLoading } = useFetchViewProperties(name);
  console.log({ view, isLoading });
  useDisableNavBar();
  useGlobalStyleReset();
  if (isLoading) {
    return <Spinner />;
  }
  if (view?.type === "search-alias") {
    return (
      <ChakraCustomProvider overrideNonReact>
        <SearchViewsCustomStyleReset>
          <SearchAliasViewSettings
            view={view as SearchAliasViewPropertiesType}
          />
        </SearchViewsCustomStyleReset>
      </ChakraCustomProvider>
    );
  }
  if (view?.type === "arangosearch") {
    return (
      <ChakraCustomProvider overrideNonReact>
        <SearchViewsCustomStyleReset>
          <ArangoSearchViewSettings
            view={view as ArangoSearchViewPropertiesType}
          />
        </SearchViewsCustomStyleReset>
      </ChakraCustomProvider>
    );
  }
  return null;
};
