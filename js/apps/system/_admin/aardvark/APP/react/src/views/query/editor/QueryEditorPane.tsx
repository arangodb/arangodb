import { Box, Divider, Grid } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { QueryEditorBottomBar } from "./QueryEditorBottomBar";
import { QueryEditorTopBar } from "./QueryEditorTopBar";
import { QueryResults } from "./QueryResults";
import { QuerySettingsPane } from "./QuerySettingsPane";
import { SavedQueryView } from "./SavedQueryView";

export const QueryEditorPane = () => {
  const { currentView, queryValue, onQueryValueChange } = useQueryContext();
  if (currentView === "saved") {
    return (
      <>
        <SavedQueryView />
        <QueryResults />
      </>
    );
  }
  return (
    <Box background="white" height="full">
      <QueryEditorTopBar />
      <Grid gridTemplateColumns="1.4fr 4px 0.6fr" height="calc(100% - 300px)">
        <AQLEditor defaultValue={queryValue} onChange={onQueryValueChange} />
        <Divider
          borderLeftWidth="4px"
          borderColor="gray.400"
          orientation="vertical"
        />
        <QuerySettingsPane />
      </Grid>
      <Divider borderColor="gray.400" borderBottomWidth="4px" />
      <QueryEditorBottomBar />
      <QueryResults />
    </Box>
  );
};
