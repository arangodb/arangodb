import { ArrowBackIcon, CopyIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Grid,
  IconButton,
  Spinner,
  Stack,
  useDisclosure
} from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { InfoCircle } from "styled-icons/boxicons-solid";
import { PlayArrow } from "styled-icons/material";
import momentMin from "../../../../../frontend/js/lib/moment.min";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { download } from "../../../utils/downloadHelper";
import { useQueryContext } from "../QueryContextProvider";
import { AQLEditor } from "./AQLEditor";
import { DeleteQueryModal } from "./DeleteQueryModal";
import { ImportQueryModal } from "./ImportQueryModal";
import { QueryType } from "./useFetchUserSavedQueries";

export const SavedQueryView = () => {
  const { savedQueries, isFetchingQueries } = useQueryContext();

  return (
    <Box background="white">
      <SavedQueryToolbar />
      <Grid gridTemplateRows="1fr 60px">
        <Grid gridTemplateColumns="minmax(450px, 0.5fr) 1fr">
          {isFetchingQueries ? (
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
const columnHelper = createColumnHelper<QueryType>();

const ActionCell = (info: CellContext<QueryType, unknown>) => {
  const { onQueryChange, setCurrentView, onExecute, onExplain } =
    useQueryContext();
  const query = info.row.original;
  const {
    isOpen: isDeleteModalOpen,
    onOpen: onOpenDeleteModal,
    onClose: onCloseDeleteModal
  } = useDisclosure();
  return (
    <Box whiteSpace="nowrap">
      <IconButton
        variant="ghost"
        icon={<CopyIcon />}
        aria-label={`Copy ${query.name}`}
        size="sm"
        colorScheme="gray"
        onClick={() => {
          onQueryChange({
            value: query.value,
            parameter: query.parameter || {},
            name: query.name
          });

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
        onClick={onOpenDeleteModal}
      />
      <DeleteQueryModal
        query={info.row.getIsSelected() ? query : null}
        isOpen={isDeleteModalOpen}
        onClose={onCloseDeleteModal}
      />
    </Box>
  );
};

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("modified_at", {
    header: "Modified At",
    id: "modified_at",
    enableColumnFilter: false,
    cell: info => {
      const cellValue = info.cell.getValue();
      return cellValue
        ? momentMin(cellValue).format("YYYY-MM-DD HH:mm:ss")
        : "-";
    }
  }),
  columnHelper.accessor("actions" as any, {
    header: "Actions",
    id: "actions",
    enableSorting: false,
    cell: ActionCell,
    enableColumnFilter: false
  })
];

const SavedQueryTable = ({ savedQueries }: { savedQueries: QueryType[] }) => {
  const tableInstance = useSortableReactTable<QueryType>({
    columns: TABLE_COLUMNS,
    data: savedQueries.reverse(),
    initialRowSelection: {
      0: true
    },
    enableRowSelection: true,
    enableMultiRowSelection: false
  });
  const selectedRowModel = tableInstance.getSelectedRowModel();
  const selectedQuery = selectedRowModel.rows[0]?.original;
  return (
    <>
      <Stack>
        <FiltersList<QueryType> columns={TABLE_COLUMNS} table={tableInstance} />
        <ReactTable<QueryType>
        table={tableInstance}
        onRowSelect={row => {
          if (!row.getIsSelected()) {
            row.toggleSelected();
          }
        }}
      />
      </Stack>
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
        <AQLEditor isPreview value={query.value} />
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

const SavedQueryBottomBar = () => {
  const {
    isOpen: isImportModalOpen,
    onOpen: onOpenImportModal,
    onClose: onCloseImportModal
  } = useDisclosure();
  const onDownload = () => {
    download(`query/download/${window.App.currentUser}`);
  };
  return (
    <>
      <ImportQueryModal
        isOpen={isImportModalOpen}
        onClose={onCloseImportModal}
      />
      <Stack
        padding="2"
        direction="row"
        justifyContent="flex-end"
        alignItems="center"
      >
        <Button size="sm" colorScheme="gray" onClick={onOpenImportModal}>
          Import Queries
        </Button>
        <Button size="sm" colorScheme="gray" onClick={onDownload}>
          Export Queries
        </Button>
      </Stack>
    </>
  );
};
