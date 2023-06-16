import { Box, Tab, TabList, TabPanel, TabPanels, Tabs } from "@chakra-ui/react";
import React from "react";
import { ChakraCustomProvider } from "../../theme/ChakraCustomProvider";
import { useDisableNavBar } from "../../utils/useDisableNavBar";
import { useGlobalStyleReset } from "../../utils/useGlobalStyleReset";
import { EditorPane } from "./EditorPane";
import { RunningQueries } from "./RunningQueries";
import { SlowQueryHistory } from "./SlowQueryHistory";

export const QueryViewWrap = () => {
  useDisableNavBar();
  useGlobalStyleReset({
    removeContentDiv: true
  });
  return (
    <ChakraCustomProvider>
      <Box height="calc(100vh - 60px)" bg="gray.100" overflow="auto">
        <Tabs isLazy>
          <TabList>
            <Tab>Editor</Tab>
            <Tab>Running Queries</Tab>
            <Tab>Slow Query History</Tab>
          </TabList>
          <TabPanels>
            <TabPanel>
              <EditorPane />
            </TabPanel>
            <TabPanel>
              <RunningQueries />
            </TabPanel>
            <TabPanel>
              <SlowQueryHistory />
            </TabPanel>
          </TabPanels>
        </Tabs>
      </Box>
    </ChakraCustomProvider>
  );
};
