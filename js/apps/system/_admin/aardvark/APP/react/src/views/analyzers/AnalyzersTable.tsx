import { Link } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React, { useMemo } from "react";
import { Link as RouterLink, useHistory } from "react-router-dom";
import { ReactTable } from "../../components/table/ReactTable";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "./AnalyzersHelpers";
import { FiltersList, FilterType } from "../../components/table/FiltersList";

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
    }
  }),
  columnHelper.accessor("name", {
    header: "DB",
    id: "db",
    cell: info => {
      const cellValue = info.cell.getValue();
      return cellValue.includes("::") ? cellValue.split("::")[0] : "_system";
    }
  }),
  columnHelper.accessor("type", {
    header: "Type",
    id: "type",
    cell: info => {
      return TYPE_TO_LABEL_MAP[info.cell.getValue()] || info.cell.getValue();
    }
  })
];

const TABLE_FILTERS = TABLE_COLUMNS.map(column => {
  if (column.id === "db") {
    return {
      ...column,
      filterType: "single-select",
      getValue: (value: string) => {
        return value.includes("::") ? value.split("::")[0] : "_system";
      }
    };
  }
  return {
    ...column,
    filterType: "single-select"
  };
}).filter(Boolean) as FilterType[];

export const AnalyzersTable = () => {
  const { analyzers, showSystemAnalyzers } = useAnalyzersContext();
  const newAnalyzers = useMemo(() => {
    return analyzers?.filter(
      analyzer => analyzer.name.includes("::") || showSystemAnalyzers
    );
  }, [analyzers, showSystemAnalyzers]);
  const history = useHistory();
  return (
    <>
      <ReactTable
        initialSorting={[
          {
            id: "name",
            desc: false
          }
        ]}
        columns={TABLE_COLUMNS}
        data={newAnalyzers || []}
        emptyStateMessage="No analyzers found"
        onRowSelect={row => {
          history.push(`/analyzers/${row.original.name}`);
        }}
      >
        {({ table }) => {
          return <FiltersList filters={TABLE_FILTERS} table={table} />;
        }}
      </ReactTable>
    </>
  );
};
