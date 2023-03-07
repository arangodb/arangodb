import { Spinner } from "@chakra-ui/react";
import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
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
  useDisableNavBar();
  useGlobalStyleReset();
  if (isLoading) {
    return <Spinner />;
  }
  if (view?.type === "search-alias") {
    return (
      <ChakraCustomProvider>
        <SearchAliasViewSettings view={view} />
      </ChakraCustomProvider>
    );
  }
  return (
    <ChakraCustomProvider>
      <ViewSettings name={name} isCluster={isCluster} />
    </ChakraCustomProvider>
  );
};
