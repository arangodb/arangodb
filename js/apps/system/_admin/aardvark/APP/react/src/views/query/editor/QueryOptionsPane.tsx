import {
  Box,
  Button,
  Tab,
  TabList,
  TabPanel,
  TabPanels,
  Tabs
} from "@chakra-ui/react";
import React, { useState } from "react";
import { BindVariablesTab } from "./BindVariablesTab";
import { QueryOptionsTab } from "./QueryOptionsTab";

export const QueryOptionsPane = () => {
  const [mode, setMode] = useState<"json" | "table">("table");
  return (
    <Box height="full" overflow="hidden">
      <Tabs height="full" size="sm">
        <TabList alignItems="center" paddingRight="2">
          <Tab>Bind Variables</Tab>
          <Tab>Options</Tab>
          <Button
            marginLeft="auto"
            size="xs"
            colorScheme="gray"
            onClick={() => {
              setMode(mode === "json" ? "table" : "json");
            }}
          >
            {mode === "json" ? "Show Table" : "Show JSON"}
          </Button>
        </TabList>
        <TabPanels height="calc(100% - 32px)" overflow="auto">
          <TabPanel padding="0" height="full">
            <BindVariablesTab mode={mode} />
          </TabPanel>
          <TabPanel padding="0" height="full">
            <QueryOptionsTab mode={mode} />
          </TabPanel>
        </TabPanels>
      </Tabs>
    </Box>
  );
};
