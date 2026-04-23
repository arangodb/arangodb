import { Resizable } from "re-resizable";
import { Box, Divider, Flex, Grid } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import React, { useCallback } from "react";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { QueryEditorBottomBar } from "./QueryEditorBottomBar";
import { QueryEditorTopBar } from "./QueryEditorTopBar";
import { QueryOptionsPane } from "./QueryOptionsPane";
import { QueryResults } from "../queryResults/QueryResults";
import { SavedQueryView } from "./SavedQueryView";

const AQL_EDITOR_CONTAINER_CLASS = "aql-editor-container";

export const QueryEditorPane = () => {
  const { currentView, queryValue, onQueryValueChange, resetEditor, aqlJsonEditorRef } =
    useQueryContext();
  const refreshEditorSize = useCallback(() => {
    requestAnimationFrame(() => {
      const aceEditor = (aqlJsonEditorRef.current as any)?.jsonEditor
        ?.aceEditor;
      if (!aceEditor) return;
      aceEditor.resize(true);
      aceEditor.renderer.updateFull(true);
    });
  }, [aqlJsonEditorRef]);
  if (currentView === "saved") {
    return (
      <>
        <SavedQueryView />
        <QueryResults />
      </>
    );
  }
  return (
    <Box background="white" height="100%" overflow="hidden">
      <Global
        styles={{
          [`.${AQL_EDITOR_CONTAINER_CLASS} .jsoneditor, .${AQL_EDITOR_CONTAINER_CLASS} .jsoneditor > div, .${AQL_EDITOR_CONTAINER_CLASS} .jsoneditor-outer`]: {
            height: "100%",
            overflow: "hidden"
          }
        }}
      />
      <Resizable
        defaultSize={{ width: "100%", height: "90%" }}
        minHeight={270}
        maxHeight="100%"
        enable={{ bottom: true }}
        handleStyles={{ bottom: { zIndex: 1 } }}
        handleComponent={{ bottom: <HandleComponent /> }}
        onResize={refreshEditorSize}
        onResizeStop={refreshEditorSize}
      >
        <Flex height="100%" minHeight={0} direction="column" overflow="hidden">
          <QueryEditorTopBar />
          <Grid
            gridTemplateColumns="1.4fr 4px 0.6fr"
            height="calc(100% - 58px - 58px)"
            minHeight={0}
            overflow="hidden"
          >
            <Box className={AQL_EDITOR_CONTAINER_CLASS} minHeight={0} minWidth={0} overflow="hidden">
              <AQLEditor
                autoFocus
                resetEditor={resetEditor}
                value={queryValue}
                onChange={onQueryValueChange}
              />
            </Box>
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
