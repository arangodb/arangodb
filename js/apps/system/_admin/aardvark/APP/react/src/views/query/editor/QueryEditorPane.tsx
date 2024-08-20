import { Resizable } from "re-resizable";
import { Box, Divider, Flex, Grid } from "@chakra-ui/react";
import React from "react";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { QueryEditorBottomBar } from "./QueryEditorBottomBar";
import { QueryEditorTopBar } from "./QueryEditorTopBar";
import { QueryOptionsPane } from "./QueryOptionsPane";
import { QueryResults } from "../queryResults/QueryResults";
import { SavedQueryView } from "./SavedQueryView";

export const QueryEditorPane = () => {
  const {
    currentView,
    queryValue,
    onQueryValueChange,
    resetEditor,
    setResetEditor
  } = useQueryContext();
  if (currentView === "saved") {
    return (
      <>
        <SavedQueryView />
        <QueryResults />
      </>
    );
  }
  return (
    <Box background="white" height="100%">
      <Resizable
        defaultSize={{ width: "100%", height: "90%" }}
        minHeight={270}
        maxHeight="100%"
        enable={{ bottom: true }}
        handleStyles={{ bottom: { zIndex: 1 } }}
        handleComponent={{ bottom: <HandleComponent /> }}
        onResizeStop={() => {
          setResetEditor(!resetEditor);
        }}
      >
        <Flex height="100%" direction="column">
          <QueryEditorTopBar />
          <Grid
            gridTemplateColumns="1.4fr 4px 0.6fr"
            height="calc(100% - 58px - 58px)"
            flexShrink={1}
          >
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
        </Flex>
      </Resizable>
      <QueryResults />
    </Box>
  );
};

const HandleComponent = () => {
  return (
    <Box height="100%" width="100%" position="relative" role="group">
      <Box
        position="relative"
        top="4px"
        height="1px"
        backgroundColor="gray.300"
        _groupHover={{
          backgroundColor: "blue.400"
        }}
        _groupActive={{
          backgroundColor: "blue.600"
        }}
      />
      <Box
        height="2px"
        borderRadius="sm"
        margin="auto"
        position="relative"
        top="-2px"
        backgroundColor="gray.300"
        width="20px"
        _groupHover={{
          backgroundColor: "blue.400"
        }}
        _groupActive={{
          backgroundColor: "blue.600"
        }}
      />
      <Box
        height="2px"
        borderRadius="sm"
        margin="auto"
        position="relative"
        top="5px"
        backgroundColor="gray.300"
        width="20px"
        _groupHover={{
          backgroundColor: "blue.400"
        }}
        _groupActive={{
          backgroundColor: "blue.600"
        }}
      />
    </Box>
  );
};
