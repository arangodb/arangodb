import { EditIcon } from "@chakra-ui/icons";
import { Button, Link, Stack, useDisclosure } from "@chakra-ui/react";
import { CellContext, createColumnHelper } from "@tanstack/react-table";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { Link as RouterLink } from "react-router-dom";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { EditGraphModal } from "./EditGraphModal";
import { detectGraphType } from "./graphListHelpers";
import { useGraphsListContext } from "./GraphsListContext";

const columnHelper = createColumnHelper<GraphInfo>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/graphs-v2/${encodeURIComponent(cellValue)}`}
          textDecoration="underline"
          color="blue.500"
          _hover={{
            color: "blue.600"
          }}
        >
          {cellValue}
        </Link>
      );
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor(
    row => {
      const { label } = detectGraphType(row);
      return label;
    },
    {
      header: "Type",
      id: "type",
      filterFn: "equals"
    }
  ),
  columnHelper.accessor("actions" as any, {
    header: "Actions",
    id: "actions",
    enableColumnFilter: false,
    enableSorting: false,
    cell: info => {
      return <ActionCell info={info} />;
    }
  })
];

const ActionCell = ({ info }: { info: CellContext<GraphInfo, any> }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();

  return (
    <>
      <EditGraphModal
        isOpen={isOpen}
        onClose={onClose}
        graph={info.cell.row.original}
      />
      <Button
        onClick={e => {
          e.stopPropagation();
          e.preventDefault();
          onOpen();
        }}
        size="xs"
      >
        <EditIcon />
      </Button>
    </>
  );
};

export const GraphsTable = () => {
  const { graphs } = useGraphsListContext();
  const tableInstance = useSortableReactTable<GraphInfo>({
    data: graphs || [],
    columns: TABLE_COLUMNS,
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    storageKey: "graphs"
  });

  return (
    <>
      <Stack>
        <TableControl<GraphInfo>
          table={tableInstance}
          columns={TABLE_COLUMNS}
        />
        <ReactTable<GraphInfo>
          table={tableInstance}
          emptyStateMessage="No graphs found"
          onRowSelect={row => {
            window.location.hash = `#graphs-v2/${encodeURIComponent(
              row.original.name
            )}`;
          }}
        />
      </Stack>
    </>
  );
};
