import { Spinner } from "@chakra-ui/react";
import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { ViewSettings } from "./ViewSettings";
import { SearchAliasViewSettings } from "./searchAliasView/SearchAliasViewSettings";
import { useFetchViewProperties } from "./searchAliasView/useFetchViewProperties";
import { SearchViewsCustomStyleReset } from "./SearchViewsCustomStyleReset";

export const ViewSettingsWrap = ({ name }: { name: string }) => {
  const { view, isLoading } = useFetchViewProperties(name);
  useDisableNavBar();
  useGlobalStyleReset();
  if (isLoading) {
    return <Spinner />;
  }
  if (view?.type === "search-alias") {
    return (
      <ChakraCustomProvider overrideNonReact>
        <SearchViewsCustomStyleReset>
          <SearchAliasViewSettings view={view} />
        </SearchViewsCustomStyleReset>
      </ChakraCustomProvider>
    );
  }
  return (
    <ChakraCustomProvider overrideNonReact>
      <SearchViewsCustomStyleReset>
        <ViewSettings name={name} />
      </SearchViewsCustomStyleReset>
    </ChakraCustomProvider>
  );
};
