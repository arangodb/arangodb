import { DeleteIcon, ViewIcon } from "@chakra-ui/icons";
import {
  Button,
  IconButton,
  ModalBody,
  ModalFooter,
  ModalHeader,
  Stack,
  useDisclosure
} from "@chakra-ui/react";
import { createColumnHelper, Row } from "@tanstack/react-table";
import { AnalyzerDescription } from "arangojs/analyzer";
import React, { useMemo } from "react";
import { mutate } from "swr";
import { Modal } from "../../components/modal";
import { getCurrentDB } from "../../utils/arangoClient";
import { useAnalyzersContext } from "./AnalyzersContext";
import { TYPE_TO_LABEL_MAP } from "./AnalyzersHelpers";
import { FiltersList, FilterType } from "./FiltersList";
import { ReactTable } from "../../components/table/ReactTable";
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
    header: "Name",
    id: "name"
  }),
  columnHelper.accessor("type", {
    header: "Type",
    id: "type",
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

const TABLE_FILTERS = TABLE_COLUMNS.map(column => {
  if (column.id === "actions") {
    return null;
  }
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
const RowActions = ({ row }: { row: Row<AnalyzerDescription> }) => {
  const { setViewAnalyzerName } = useAnalyzersContext();
  const { isOpen: showDeleteModal, onClose, onOpen } = useDisclosure();
  return (
    <>
      <Stack direction="row">
        <IconButton
          size="sm"
          aria-label="View"
          variant="ghost"
          icon={<ViewIcon />}
          onClick={() => {
            setViewAnalyzerName(row.original.name);
          }}
        />
        <IconButton
          size="sm"
          variant="ghost"
          colorScheme="red"
          aria-label="Delete"
          icon={<DeleteIcon />}
          onClick={() => {
            onOpen();
          }}
        />
      </Stack>
      {showDeleteModal && <DeleteAnalyzerModal onClose={onClose} row={row} />}
    </>
  );
};

const DeleteAnalyzerModal = ({
  row,
  onClose
}: {
  row: Row<AnalyzerDescription>;
  onClose: () => void;
}) => {
  const [isLoading, setIsLoading] = React.useState(false);
  const onDelete = async () => {
    const currentDB = getCurrentDB();
    setIsLoading(true);
    await currentDB.analyzer(row.original.name).drop();
    mutate("/analyzers");
    setIsLoading(false);
    onClose();
  };
  return (
    <Modal isOpen onClose={onClose}>
      <ModalHeader>Delete Analyzer</ModalHeader>
      <ModalBody>
        Are you sure you want to delete {row.original.name}?
      </ModalBody>
      <ModalFooter>
        <Stack direction="row">
          <Button
            isDisabled={isLoading}
            colorScheme="gray"
            onClick={() => onClose()}
          >
            Cancel
          </Button>
          <Button isLoading={isLoading} colorScheme="red" onClick={onDelete}>
            Delete
          </Button>
        </Stack>
      </ModalFooter>
    </Modal>
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
      >
        {({ table }) => {
          return <FiltersList filters={TABLE_FILTERS} table={table} />;
        }}
      </ReactTable>
      <ViewAnalyzerModal />
    </>
  );
};
