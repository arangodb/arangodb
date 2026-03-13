import { ReactTable, TableControl, useSortableReactTable } from "@arangodb/ui";
import { CollectionMetadata } from "arangojs/collection";
import { Flex, Link } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { ListHeader } from "../../../components/table/ListHeader";
import { useCollectionsContext } from "../CollectionsContext";
import { STATUS_TO_LABEL_MAP, TYPE_TO_LABEL_MAP } from "../CollectionsHelpers";

const columnHelper = createColumnHelper<CollectionMetadata>();

const TABLE_COLUMNS = [
  columnHelper.accessor("name", {
    header: "Name",
    id: "name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          href={`#cInfo/${encodeURIComponent(cellValue)}`}
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
  columnHelper.accessor(row => TYPE_TO_LABEL_MAP[row.type], {
    header: "Type",
    id: "type",
    meta: {
      filterType: "single-select"
    }
  }),
  columnHelper.accessor(
    row => {
      return row.isSystem ? "System" : "Custom";
    },
    {
      header: "Source",
      id: "source",
      meta: {
        filterType: "single-select"
      }
    }
  ),
  columnHelper.accessor(
    row => STATUS_TO_LABEL_MAP[row.status],
    {
      header: "Status",
      id: "status",
      filterFn: "arrIncludesSome",
      meta: {
        filterType: "multi-select"
      }
    }
  )
];

export const CollectionsTable = ({
  onAddCollectionClick
}: {
  onAddCollectionClick: () => void;
}) => {
  const { collections } = useCollectionsContext();
  const tableInstance = useSortableReactTable<CollectionMetadata>({
    data: collections || [],
    columns: TABLE_COLUMNS,
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
    ],
    storageKey: "collections"
  });
  return (
    <Flex direction="column" gap="2">
      <Flex direction="column" gap="4">
        <CollectionTableHeader onAddCollectionClick={onAddCollectionClick} />
        <TableControl<CollectionMetadata> table={tableInstance} />
      </Flex>
      <ReactTable<CollectionMetadata>
        table={tableInstance}
        emptyStateMessage="No collections found"
        onRowSelect={row => {
          window.location.hash = `#cInfo/${encodeURIComponent(
            row.original.name
          )}`;
        }}
      />
    </Flex>
  );
};
const CollectionTableHeader = ({
  onAddCollectionClick
}: {
  onAddCollectionClick: () => void;
}) => {
  return (
    <ListHeader
      onButtonClick={onAddCollectionClick}
      heading="Collections"
      buttonText="Add collection"
    />
  );
};
