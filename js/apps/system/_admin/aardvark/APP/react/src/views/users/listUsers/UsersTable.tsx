import { Link, Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import { CreateDatabaseUser } from "arangojs/database";
import React from "react";
import { Link as RouterLink } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { useUsersContext } from "../UsersContext";
//import { detectType } from "../GraphsHelpers";
//import { UsersModeProvider } from "../UsersModeContext";
//import { EditUserModal } from "./EditUserModal";

const columnHelper = createColumnHelper<CreateDatabaseUser>();

const TABLE_COLUMNS = [
  columnHelper.accessor("username", {
    header: "Username",
    id: "username",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <Link
          as={RouterLink}
          to={`/user/${cellValue}`}
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
  /*
  columnHelper.accessor(row => getGraphTypeString(row), {
    header: "Type",
    id: "type",
    filterFn: "equals"
  }),
  columnHelper.accessor("actions" as any, {
    header: "Actions",
    id: "actions",
    enableColumnFilter: false,
    enableSorting: false,
    cell: info => {
      return <ActionCell info={info} />;
    }
  })
  */
];

/*
const ActionCell = ({ info }: { info: CellContext<GraphInfo, any> }) => {
  const { isOpen, onOpen, onClose } = useDisclosure();
  console.log("info in ActionCell: ", info);

  return (
    <>
      <GraphsModeProvider mode="edit">
        <EditGraphModal
          isOpen={isOpen}
          onClose={onClose}
          graph={info.cell.row.original}
        />
      </GraphsModeProvider>
      <Button onClick={onOpen} size="xs">
        <EditIcon />
      </Button>
    </>
  );
};
*/

/*
const getGraphTypeString = (graph: GraphInfo) => {
  const { type, isDisjoint } = detectType(graph);

  if (type === "satellite") {
    if (isDisjoint) {
      return "Disjoint Sattelite";
    }
    return "Satellite";
  }

  if (type === "smart") {
    if (isDisjoint) {
      return "Disjoint Smart";
    }
    return "Smart";
  }

  if (type === "enterprise") {
    if (isDisjoint) {
      return "Disjoint Enterprise";
    }
    return "Enterprise";
  }

  return "General";
};
*/

export const UsersTable = () => {
  const { users } = useUsersContext();
  console.log("users: ", users);
  const tableInstance = useSortableReactTable<CreateDatabaseUser>({
    data: users || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "username",
        desc: false
      }
    ]
  });

  return (
    <>
      <Stack>
        <FiltersList<CreateDatabaseUser>
          columns={TABLE_COLUMNS}
          table={tableInstance}
        />
        <ReactTable<CreateDatabaseUser>
          table={tableInstance}
          emptyStateMessage="No users found"
        />
      </Stack>
    </>
  );
};
