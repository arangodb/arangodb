import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { GenericAnalyzerDescription } from "arangojs/analyzer";
import React from "react";
import { useHistory } from "react-router-dom";
import { ReactTable } from "../../../components/table/ReactTable";
import { TableControl } from "../../../components/table/TableControl";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { encodeHelper } from "../../../utils/encodeHelper";
import { AnalyzerTypes } from "../Analyzer.types";
import { useAnalyzersContext } from "../AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "../AnalyzersHelpers";

type AnalyzerDataType = GenericAnalyzerDescription & {
  type: AnalyzerTypes;
};

const columnHelper = createColumnHelper<AnalyzerDataType>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      const { encoded: encodedAnalyzerName } = encodeHelper(cellValue);
      // need to use href here instead of RouteLink due to a bug in react-router
      return (
        <Link
          href={`#analyzers/${encodedAnalyzerName}`}
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
  const tableInstance = useSortableReactTable<AnalyzerDataType>({
    data: analyzers || [],
    columns: TABLE_COLUMNS,
    storageKey: "analyzers",
    defaultSorting: [
      {
        id: "name",
        desc: false
      }
    ],
    defaultFilters: [
      {
        id: "source",
        value: "Custom"
      }
    ]
  });
  const history = useHistory();
  return (
    <Stack>
      <TableControl<AnalyzerDataType>
        table={tableInstance}
        columns={TABLE_COLUMNS}
      />
      <ReactTable<AnalyzerDataType>
        table={tableInstance}
        emptyStateMessage="No analyzers found"
        onRowSelect={row => {
          const { encoded: encodedAnalyzerName } = encodeHelper(
            row.original.name
          );

          history.push(`/analyzers/${encodedAnalyzerName}`);
        }}
      />
    </Stack>
  );
};
