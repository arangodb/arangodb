import { ArrowBackIcon, CopyIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Grid,
  IconButton,
  Spinner,
  Stack,
  Table,
  TableContainer,
  Tbody,
  Td,
  Th,
  Thead,
  Tr
} from "@chakra-ui/react";
import { ArangoUser } from "arangojs/database";
import React, { useState } from "react";
import { InfoCircle } from "styled-icons/boxicons-solid";
import { PlayArrow } from "styled-icons/material";
import useSWR from "swr";
import momentMin from "../../../../../frontend/js/lib/moment.min";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";

export const SavedQueryView = () => {
  const { savedQueries, isLoading } = useFetchUserSavedQueries();

  return (
    <Box background="white">
      <SavedQueryToolbar />
      <Grid gridTemplateRows="1fr 60px">
        <Grid gridTemplateColumns="minmax(450px, 0.5fr) 1fr">
          {isLoading ? (
            <Spinner />
          ) : (
            <SavedQueryTable savedQueries={savedQueries as QueryType[]} />
          )}
        </Grid>
        <SavedQueryBottomBar />
      </Grid>
    </Box>
  );
};

const SavedQueryToolbar = () => {
  const { setCurrentView, currentView } = useQueryContext();
  return (
    <Stack direction="row" padding="2">
      <Button
        leftIcon={<ArrowBackIcon />}
        size="sm"
        colorScheme="gray"
        onClick={() => {
          setCurrentView(currentView === "saved" ? "editor" : "saved");
        }}
      >
        Back
      </Button>
    </Stack>
  );
};

type QueryType = {
  name: string;
  value: string;
  modified_at: number;
  parameter: any;
};
const SavedQueryTable = ({ savedQueries }: { savedQueries: QueryType[] }) => {
  const [selectedQuery, setSelectedQuery] = useState<QueryType | null>(
    savedQueries[0]
  );
  const {
    onQueryChange,
    setCurrentView,
    onBindParamsChange,
    onExecute,
    onExplain
  } = useQueryContext();
  return (
    <>
      <TableContainer height="400px" overflowX="auto" overflowY="auto">
        <Table size="sm">
          <Thead>
            <Th>Name</Th>
            <Th>Modified At</Th>
            <Th>Actions</Th>
          </Thead>
          <Tbody>
            {savedQueries.map(query => (
              <Tr
                key={query.name}
                backgroundColor={
                  selectedQuery?.name === query.name ? "gray.200" : undefined
                }
                _hover={{
                  background: "gray.100",
                  cursor: "pointer"
                }}
                onClick={() => {
                  setSelectedQuery(query);
                }}
              >
                <Td>{query.name}</Td>
                <Td>
                  {query.modified_at
                    ? momentMin(query.modified_at).format("YYYY-MM-DD HH:mm:ss")
                    : "-"}
                </Td>
                <Td>
                  <IconButton
                    variant="ghost"
                    icon={<CopyIcon />}
                    aria-label={`Copy ${query.name}`}
                    size="sm"
                    colorScheme="gray"
                    onClick={() => {
                      onQueryChange(query.value);
                      onBindParamsChange(query.parameter);
                      setCurrentView("editor");
                    }}
                    title="Copy"
                  />
                  <IconButton
                    variant="ghost"
                    icon={<InfoCircle width="16px" height="16px" />}
                    aria-label={`Explain ${query.name}`}
                    size="sm"
                    colorScheme="gray"
                    onClick={() => {
                      onExplain({
                        queryValue: query.value,
                        queryBindParams: query.parameter
                      });
                    }}
                    title="Explain"
                  />
                  <IconButton
                    variant="ghost"
                    icon={<PlayArrow width="16px" height="16px" />}
                    aria-label={`Execute ${query.name}`}
                    size="sm"
                    colorScheme="green"
                    onClick={() => {
                      onExecute({
                        queryValue: query.value,
                        queryBindParams: query.parameter
                      });
                    }}
                    title="Execute"
                  />
                  <IconButton
                    variant="ghost"
                    icon={<DeleteIcon />}
                    aria-label={`Delete ${query.name}`}
                    size="sm"
                    colorScheme="red"
                    title="Delete"
                  />
                </Td>
              </Tr>
            ))}
          </Tbody>
        </Table>
      </TableContainer>
      <QueryPreview query={selectedQuery} />
    </>
  );
};

const QueryPreview = ({ query }: { query: QueryType | null }) => {
  if (!query) {
    return <Box>Select a query to view</Box>;
  }
  return (
    <Grid gridTemplateColumns="1fr 1fr">
      <Stack>
        <Box fontWeight="medium" fontSize="md">
          Preview: {query.name}
        </Box>
        <AQLEditor
          isPreview
          defaultValue={query.value}
          onChange={() => {
            //noop
          }}
        />
      </Stack>
      <Stack>
        <Box fontWeight="medium" fontSize="sm">
          Bind Variables
        </Box>
        <ControlledJSONEditor
          mode="code"
          isReadOnly
          value={query.parameter}
          mainMenuBar={false}
          htmlElementProps={{
            style: {
              height: "100%"
            }
          }}
        />
      </Stack>
    </Grid>
  );
};

const useFetchUserSavedQueries = () => {
  const fetchUser = async () => {
    const currentDB = getCurrentDB();
    const path = `/_api/user/${encodeURIComponent(window.App.currentUser)}`;
    const user = await currentDB.request({
      absolutePath: false,
      path
    });
    const savedQueries = (user.body as ArangoUser).extra.queries;
    return savedQueries;
  };
  const { data, isLoading } = useSWR<QueryType[]>("/savedQueries", fetchUser);
  return { savedQueries: data, isLoading };
};

const SavedQueryBottomBar = () => {
  return (
    <Stack
      padding="2"
      direction="row"
      justifyContent="flex-end"
      alignItems="center"
    >
      <Button size="sm" colorScheme="gray">
        Import Queries
      </Button>
      <Button size="sm" colorScheme="gray">
        Export Queries
      </Button>
    </Stack>
  );
};
