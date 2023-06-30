import { Box, Link } from "@chakra-ui/react";
import { GraphInfo } from "arangojs/graph";
import React from "react";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { useGraphsContext } from "../GraphsContext";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { createColumnHelper } from "@tanstack/react-table";
import { ReactTable } from "../../../components/table/ReactTable";

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
          to={`/graphs-v2/${cellValue}`}
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
  })
];

export const GraphsTable = () => {
  const { graphs } = useGraphsContext();
  const tableInstance = useSortableReactTable<GraphInfo>({
    data: graphs || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "name",
        desc: false
      }
    ]
  });

  const history = useHistory();

  return (
    <Box padding="4" width="100%">
      <ReactTable<GraphInfo>
        table={tableInstance}
        emptyStateMessage="No graphs found"
        onRowSelect={row => {
          history.push(`/graphs-v2/${row.original.name}`);
        }}
      />
    </Box>
  );
};
