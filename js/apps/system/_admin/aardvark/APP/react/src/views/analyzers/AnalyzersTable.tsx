import { createColumnHelper } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React, { useMemo } from "react";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_TYPE_NAME_MAP } from "./AnalyzersHelpers";
import { FilterTable } from "./FilterTable";

const columnHelper = createColumnHelper<AnalyzerDescription>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "DB",
    id: "db",
    cell: info => {
      const cellValue = info.cell.getValue();
      return cellValue.includes("::") ? cellValue.split("::")[0] : "_system";
    }
  }),
  columnHelper.accessor("name", {
    header: "Name"
  }),
  columnHelper.accessor("type", {
    header: "Type",
    cell: info => {
      return (
        TYPE_TO_TYPE_NAME_MAP[info.cell.getValue()] || info.cell.getValue()
      );
    }
  }),
  columnHelper.display({
    id: "actions",
    header: "Actions",
    cell: () => {
      return <RowActions />;
    }
  })
];

const RowActions = () => {
  return <div>View Delete</div>;
};
export const AnalyzersTable = () => {
  const { analyzers, showSystemAnalyzers } = useAnalyzersContext();
  const newAnalyzers = useMemo(() => {
    return analyzers?.filter(
      analyzer => analyzer.name.includes("::") || showSystemAnalyzers
    );
  }, [analyzers, showSystemAnalyzers]);
  return (
    <FilterTable
      initialSorting={[
        {
          id: "name",
          desc: false
        }
      ]}
      columns={TABLE_COLUMNS}
      data={newAnalyzers || []}
    />
  );
};
