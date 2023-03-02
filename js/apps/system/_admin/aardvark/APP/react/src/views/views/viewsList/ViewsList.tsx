import { Box, ChakraProvider, Grid } from "@chakra-ui/react";
import React from "react";
import { theme } from "../../../theme/theme";
import { AddViewTile } from "./AddViewTile";
import { useViewsList } from "./useViewsList";
import { ViewTile } from "./ViewTile";

export const ViewsList = () => {
  return (
    <ChakraProvider theme={theme}>
      <ViewsListInner />
    </ChakraProvider>
  );
};
const ViewsListInner = () => {
  const { viewsList } = useViewsList();
  if (!viewsList) {
    return <>no views found</>;
  }
  return (
    <Box display={"grid"} gap="4" gridTemplateColumns={"repeat(auto-fill, minmax(200px, 1fr))"}>
      <AddViewTile />
      {viewsList.map(view => {
        return <ViewTile view={view} />;
      })}
    </Box>
  );
};
