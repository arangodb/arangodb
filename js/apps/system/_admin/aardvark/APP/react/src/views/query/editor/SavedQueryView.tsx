import { ArrowBackIcon, CopyIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Grid,
  IconButton,
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
import React, { useEffect, useState } from "react";
import { InfoCircle } from "styled-icons/boxicons-solid";
import { PlayArrow } from "styled-icons/material";
import momentMin from "../../../../../frontend/js/lib/moment.min";
import { getCurrentDB } from "../../../utils/arangoClient";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";

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
export const SavedQueryView = () => {
  return (
    <Box background="white">
      <SavedQueryToolbar />
      <SavedQueryTable />
    </Box>
  );
};

const useFetchUserSavedQueries = () => {
  const [savedQueries, setSavedQueries] = useState<QueryType[]>([]);
  const fetchUser = async () => {
    const currentDB = getCurrentDB();
    const path = `/_api/user/${encodeURIComponent(window.App.currentUser)}`;
    const user = await currentDB.request({
      absolutePath: false,
      path
    });
    const savedQueries = (user.body as ArangoUser).extra.queries;
    setSavedQueries(savedQueries);
  };
  useEffect(() => {
    fetchUser();
  }, []);
  return { savedQueries };
};

type QueryType = {
  name: string;
  value: string;
  modified_at: number;
  parameter: any;
};
const SavedQueryTable = () => {
  const { savedQueries } = useFetchUserSavedQueries();
  const [selectedQuery, setSelectedQuery] = useState<QueryType | null>(null);
  return (
    <Grid gridTemplateRows="1fr 60px">
      <Grid gridTemplateColumns="0.5fr 1fr">
        <TableContainer>
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
                      ? momentMin(query.modified_at).format(
                          "YYYY-MM-DD HH:mm:ss"
                        )
                      : "-"}
                  </Td>
                  <Td>
                    <IconButton
                      variant="ghost"
                      icon={<CopyIcon />}
                      aria-label={`Copy ${query.name}`}
                      size="sm"
                      colorScheme="gray"
                    />
                    <IconButton
                      variant="ghost"
                      icon={<InfoCircle width="16px" height="16px" />}
                      aria-label={`Explain ${query.name}`}
                      size="sm"
                      colorScheme="gray"
                    >
                      Run
                    </IconButton>
                    <IconButton
                      variant="ghost"
                      icon={<PlayArrow width="16px" height="16px" />}
                      aria-label={`Run ${query.name}`}
                      size="sm"
                      colorScheme="green"
                    />
                    <IconButton
                      variant="ghost"
                      icon={<DeleteIcon />}
                      aria-label={`Delete ${query.name}`}
                      size="sm"
                      colorScheme="red"
                    />
                  </Td>
                </Tr>
              ))}
            </Tbody>
          </Table>
        </TableContainer>
        <QueryDisplay query={selectedQuery} />
      </Grid>
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
    </Grid>
  );
};

const QueryDisplay = ({ query }: { query: QueryType | null }) => {
  if (!query) {
    return <Box>Select a query to view</Box>;
  }
  return (
    <Box padding="2">
      <Box fontWeight="bold" fontSize="lg">
        {query.name}
      </Box>
      <AQLEditor
        defaultValue={query.value}
        onChange={() => {
          //noop
        }}
      />
    </Box>
  );
};
