import { Box, ChakraProvider, Spinner } from "@chakra-ui/react";
import React from "react";
import { HashRouter } from "react-router-dom";
import { theme } from "../../../theme/theme";
import { useDisableNavBar } from "../../../utils/useDisableNavBar";
import { AddViewTile } from "./AddViewTile";
import { useViewsList } from "./useViewsList";
import { ViewTile } from "./ViewTile";

export const ViewsList = () => {
  useDisableNavBar();
  return (
    <ChakraProvider theme={theme}>
      <HashRouter basename="/" hashType={"noslash"}>
        <ViewsListInner />
      </HashRouter>
    </ChakraProvider>
  );
};
const ViewsListInner = () => {
  const { viewsList, isValidating } = useViewsList();
  if (!viewsList && isValidating) {
    return <Spinner />;
  }
  if (!viewsList) {
    return <>no views found</>;
  }
  return (
    <Box
      display={"grid"}
      gap="4"
      gridTemplateColumns={"repeat(auto-fill, minmax(200px, 1fr))"}
    >
      <AddViewTile />
      {viewsList.map(view => {
        return <ViewTile key={view.globallyUniqueId} view={view} />;
      })}
    </Box>
  );
};
