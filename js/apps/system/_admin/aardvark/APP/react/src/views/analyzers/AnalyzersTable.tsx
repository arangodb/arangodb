import { DeleteIcon, ViewIcon } from "@chakra-ui/icons";
import { IconButton, Stack } from "@chakra-ui/react";
import { createColumnHelper, Row } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React, { useMemo } from "react";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "./AnalyzersHelpers";
import { FilterTable } from "./FilterTable";
import { ViewAnalyzerModal } from "./ViewAnalyzerModal";

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
      return TYPE_TO_LABEL_MAP[info.cell.getValue()] || info.cell.getValue();
    }
  }),
  columnHelper.display({
    id: "actions",
    header: "Actions",
    cell: props => {
      return <RowActions row={props.row} />;
    }
  })
];

const RowActions = ({ row }: { row: Row<AnalyzerDescription> }) => {
  const { setViewAnalyzerName } = useAnalyzersContext();
  return (
    <Stack direction="row">
      <IconButton size="sm"
        aria-label="View"
        variant="ghost"
        icon={<ViewIcon />}
        onClick={() => {
          setViewAnalyzerName(row.original.name);
        }}
      />
      <IconButton size="sm"
        variant="ghost"
        colorScheme="red"
        aria-label="Delete"
        icon={<DeleteIcon />}
        onClick={() => {
          // todo
        }}
      />
    </Stack>
  );
};
export const AnalyzersTable = () => {
  const { analyzers, showSystemAnalyzers } = useAnalyzersContext();
  const newAnalyzers = useMemo(() => {
    return analyzers?.filter(
      analyzer => analyzer.name.includes("::") || showSystemAnalyzers
    );
  }, [analyzers, showSystemAnalyzers]);
  return (
    <>
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
      <ViewAnalyzerModal />
    </>
  );
};
