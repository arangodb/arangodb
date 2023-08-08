import { Box, Divider, Grid } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { QueryEditorBottomBar } from "./QueryEditorBottomBar";
import { QueryEditorTopBar } from "./QueryEditorTopBar";
import { QueryOptionsPane } from "./QueryOptionsPane";
import { QueryResults } from "../queryResults/QueryResults";
import { SavedQueryView } from "./SavedQueryView";

export const QueryEditorPane = () => {
  const { currentView, queryValue, onQueryValueChange, resetEditor } =
    useQueryContext();
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
        <AQLEditor
          autoFocus
          resetEditor={resetEditor}
          value={queryValue}
          onChange={onQueryValueChange}
        />
        <Divider
          borderLeftWidth="4px"
          borderColor="gray.400"
          orientation="vertical"
        />
        <QueryOptionsPane />
      </Grid>
      <Divider borderColor="gray.400" borderBottomWidth="4px" />
      <QueryEditorBottomBar />
      <QueryResults />
    </Box>
  );
};
