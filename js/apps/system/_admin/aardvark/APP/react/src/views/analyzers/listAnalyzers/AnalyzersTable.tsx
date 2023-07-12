import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { useAnalyzersContext } from "../AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "../AnalyzersHelpers";

const columnHelper = createColumnHelper<AnalyzerDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/analyzers/${cellValue}`}
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
      return row.name.includes("::") ? row.name.split("::")[0] : "_system";
    },
    {
      header: "DB",
      id: "db",
      meta: {
        filterType: "single-select"
      }
    }
  ),
  columnHelper.accessor(
    row => {
      return row.name.includes("::") ? "Custom" : "Built-in";
    },
    {
      header: "Source",
      id: "source",
      meta: {
        filterType: "single-select"
      }
    }
  ),
  columnHelper.accessor(row => TYPE_TO_LABEL_MAP[row.type], {
    header: "Type",
    id: "type",
    filterFn: "arrIncludesSome",
    meta: {
      filterType: "multi-select"
    }
  })
];

export const AnalyzersTable = () => {
  const { analyzers } = useAnalyzersContext();
  const tableInstance = useSortableReactTable<AnalyzerDescription>({
    data: analyzers || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    initialFilters: [
      {
        id: "source",
        value: "Custom"
      }
    ]
  });
  const history = useHistory();
  return (
    <Stack>
      <FiltersList<AnalyzerDescription>
        columns={TABLE_COLUMNS}
        table={tableInstance}
      />
      <ReactTable<AnalyzerDescription>
        table={tableInstance}
        emptyStateMessage="No analyzers found"
        onRowSelect={row => {
          history.push(`/analyzers/${row.original.name}`);
        }}
      />
    </Stack>
  );
};
