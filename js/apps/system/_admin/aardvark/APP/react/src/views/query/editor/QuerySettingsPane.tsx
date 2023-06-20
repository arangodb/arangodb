import {
  Box,
  Button,
  Input,
  Tab,
  Table,
  TableContainer,
  TabList,
  TabPanel,
  TabPanels,
  Tabs,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import React, { useState } from "react";
import { ControlledJSONEditor } from "../../collections/indices/addIndex/invertedIndex/ControlledJSONEditor";
import { useQueryContext } from "../QueryContextProvider";

export const QuerySettingsPane = () => {
  const [mode, setMode] = useState<"json" | "table">("table");
  return (
    <Box height="full">
      <Tabs height="full" size="sm">
        <TabList alignItems="center" paddingRight="2">
          <Tab>Bind Variables</Tab>
          {/* <Tab>Options</Tab> */}
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
        <TabPanels height="full">
          <TabPanel padding="0" height="full">
            <BindVariablesTab mode={mode} />
          </TabPanel>
          {/* <TabPanel>
              <QueryOptions />
            </TabPanel> */}
        </TabPanels>
      </Tabs>
    </Box>
  );
};

const BindVariablesTab = ({ mode }: { mode: "json" | "table" }) => {
  const { queryBindParams, setQueryBindParams } = useQueryContext();
  if (mode === "table") {
    return (
      <TableContainer>
        <Table size="sm">
          <Thead>
            <Th>Key</Th>
            <Th>Value</Th>
          </Thead>
          <Tbody>
            {Object.keys(queryBindParams).map(key => (
              <Tr>
                <Td>{key}</Td>
                <Td>
                  <Input
                    size="sm"
                    placeholder="Value"
                    onChange={e => {
                      const newQueryBindParams = {
                        ...queryBindParams,
                        [key]: e.target.value
                      };
                      setQueryBindParams(newQueryBindParams);
                    }}
                  />
                </Td>
              </Tr>
            ))}
          </Tbody>
          {Object.keys(queryBindParams).length === 0 ? (
            <Tr>
              <Td colSpan={2}>No bind parameters found in query</Td>
            </Tr>
          ) : null}
        </Table>
      </TableContainer>
    );
  }
  return (
    <ControlledJSONEditor
      mode="code"
      value={queryBindParams}
      onChange={value => {
        console.log({ value });
      }}
      htmlElementProps={{
        style: {
          height: "calc(100% - 20px)"
        }
      }}
      // @ts-ignore
      mainMenuBar={false}
    />
  );
};
