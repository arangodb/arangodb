import { ArrowBackIcon, CopyIcon, DeleteIcon } from "@chakra-ui/icons";
import {
  Box,
  Button,
  Grid,
  IconButton,
  Spinner,
  Stack,
  Text,
  useDisclosure
} from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import momentMin from "moment";
import React, { useMemo } from "react";
import { InfoCircle } from "styled-icons/boxicons-solid";
import { PlayArrow } from "styled-icons/material";
import aqlTemplates from "../../../../public/assets/aqltemplates.json";
import { ControlledJSONEditor } from "../../../components/jsonEditor/ControlledJSONEditor";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
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
      <Grid gridTemplateRows="440px 48px" maxHeight="500px" overflow="hidden">
        {isFetchingQueries ? (
          <Spinner />
        ) : (
          <SavedQueryTable savedQueries={savedQueries} />
        )}
        <SavedQueryBottomBar />
      </Grid>
    </Box>
  );
};

const SavedQueryToolbar = () => {
  const { setCurrentView } = useQueryContext();
  return (
    <Stack direction="row" padding="2">
      <Button
        leftIcon={<ArrowBackIcon />}
        size="sm"
        colorScheme="gray"
        onClick={() => {
          setCurrentView("editor");
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
  const { name, value, parameter, isTemplate } = query;
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
            value,
            parameter: parameter || {},
            name
          });

          setCurrentView("editor");
        }}
        title="Copy"
      />
      {!isTemplate && (
        <>
          <IconButton
            variant="ghost"
            icon={<InfoCircle width="16px" height="16px" />}
            aria-label={`Explain ${name}`}
            size="sm"
            colorScheme="gray"
            onClick={() => {
              onExplain({
                queryValue: value,
                queryBindParams: parameter
              });
            }}
            title="Explain"
          />
          <IconButton
            variant="ghost"
            icon={<PlayArrow width="16px" height="16px" />}
            aria-label={`Execute ${name}`}
            size="sm"
            colorScheme="green"
            onClick={() => {
              onExecute({
                queryValue: value,
                queryBindParams: parameter
              });
            }}
            title="Execute"
          />
          <IconButton
            variant="ghost"
            icon={<DeleteIcon />}
            aria-label={`Delete ${name}`}
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
        </>
      )}
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
  columnHelper.accessor(row => row.created_at || 0, {
    header: "Created At",
    id: "created_at",
    enableColumnFilter: false,
    cell: info => {
      const cellValue = info.cell.getValue();
      return cellValue
        ? momentMin(cellValue).format("YYYY-MM-DD HH:mm:ss")
        : "-";
    }
  }),
  columnHelper.accessor(row => row.modified_at || 0, {
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

const SavedQueryTable = ({ savedQueries }: { savedQueries?: QueryType[] }) => {
  const finalData = useMemo(() => {
    // append aqlTemplates to savedQueries
    const updatedAqlTemplates = aqlTemplates.map(aqlTemplate => {
      return {
        ...aqlTemplate,
        isTemplate: true
      };
    });
    return (savedQueries || []).concat(
      updatedAqlTemplates as unknown as QueryType
    );
  }, [savedQueries]);
  const tableInstance = useSortableReactTable<QueryType>({
    columns: TABLE_COLUMNS,
    data: finalData,
    defaultSorting: [
      {
        id: "created_at",
        desc: true
      }
    ],
    initialRowSelection: {
      0: true
    },
    enableRowSelection: true,
    enableMultiRowSelection: false
  });
  const selectedRowModel = tableInstance.getSelectedRowModel();
  const selectedQuery = selectedRowModel.rows[0]?.original;
  return (
    <Grid
      borderBottom="4px solid"
      borderColor="gray.400"
      gridTemplateColumns="minmax(450px, 0.5fr) 1fr"
      height="full"
    >
      <Stack height="full" overflow="hidden">
        <Box paddingLeft="2" paddingTop="1">
          <TableControl
            table={tableInstance}
            columns={TABLE_COLUMNS}
            showColumnSelector={false}
          />
        </Box>
        <ReactTable<QueryType>
          table={tableInstance}
          layout="fixed"
          getCellProps={() => {
            return {
              verticalAlign: "top"
            };
          }}
          onRowSelect={row => {
            if (!row.getIsSelected()) {
              row.toggleSelected();
            }
          }}
          // 100% - height of the control element - gap
          height="calc(100% - 36px - 8px)"
          tableContainerProps={{
            // just overflow: auto doesn't work due to chakra impl details
            overflowY: "auto"
          }}
        />
      </Stack>
      <QueryPreview query={selectedQuery} />
    </Grid>
  );
};

const QueryPreview = ({ query }: { query: QueryType | null }) => {
  if (!query) {
    return <Box>Select a query to view</Box>;
  }
  return (
    <Stack height="full">
      <Box fontWeight="medium" fontSize="sm">
        Preview:{" "}
        <Text isTruncated title={query.name} maxWidth="500px">
          {query.name}
        </Text>
      </Box>
      <Grid gridTemplateColumns="1fr 1fr" height="full">
        <Stack>
          <Box fontWeight="medium" fontSize="sm">
            Value
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
    </Stack>
  );
};

const SavedQueryBottomBar = () => {
  const {
    isOpen: isImportModalOpen,
    onOpen: onOpenImportModal,
    onClose: onCloseImportModal
  } = useDisclosure();
  const onDownload = () => {
    download(`query/download/${window.App.currentUser || "root"}`);
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
