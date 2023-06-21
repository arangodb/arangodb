import { Box, Divider, Grid } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { QueryBottomBar } from "./QueryBottomBar";
import { QueryResults } from "./QueryResults";
import { QuerySettingsPane } from "./QuerySettingsPane";
import { QueryToolbar } from "./QueryToolbar";
import { SavedQueryView } from "./SavedQueryView";

export const QueryEditorPane = () => {
  const { currentView, queryValue, onQueryChange } = useQueryContext();
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
      <QueryToolbar />
      <Grid gridTemplateColumns="1.4fr 4px 0.6fr" height="calc(100% - 300px)">
        <AQLEditor defaultValue={queryValue} onChange={onQueryChange} />
        <Divider
          borderLeftWidth="4px"
          borderColor="gray.400"
          orientation="vertical"
        />
        <QuerySettingsPane />
      </Grid>
      <Divider borderColor="gray.400" borderBottomWidth="4px" />
      <QueryBottomBar />
      <QueryResults />
    </Box>
  );
};
