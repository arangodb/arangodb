import { createColumnHelper, Row } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React from "react";
import { useAnalyzersContext } from "./AnalyzersContext";
import { FilterTable } from "./FilterTable";

const columnHelper = createColumnHelper<AnalyzerDescription>();
const TABLE_HEADERS = [
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
  columnHelper.accessor("type", { header: "Type" }),
  columnHelper.display({
    id: "actions",
    header: "Actions",
    cell: props => {
      return <RowActions row={props.row} />;
    }
  })
];

const RowActions = ({ row }: { row: Row<AnalyzerDescription> }) => {
  console.log({ row });
  return <div>View Delete</div>;
};
export const AnalyzersTable = () => {
  const { analyzers } = useAnalyzersContext();
  return <FilterTable columns={TABLE_HEADERS} data={analyzers || []} />;
};
