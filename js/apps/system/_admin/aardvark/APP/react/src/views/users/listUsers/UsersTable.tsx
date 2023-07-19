import { Stack } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import React from "react";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { DatabaseUserValues } from "../addUser/CreateUser.types";
import { useUsersContext } from "../UsersContext";
import { AvatarCell, LinkCell, StatusCell } from "./UserTableCells";

const columnHelper = createColumnHelper<DatabaseUserValues>();

const TABLE_COLUMNS = [
  columnHelper.accessor("user", {
    header: "Username",
    id: "user",
    cell: info => {
      return <LinkCell info={info} />;
    },
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("extra.name", {
    header: "Name",
    id: "extra.name",
    meta: {
      filterType: "text"
    }
  }),
  columnHelper.accessor("extra.img", {
    header: "Gravatar",
    id: "extra.img",
    enableColumnFilter: false,
    enableSorting: false,
    cell: info => {
      return <AvatarCell info={info} />;
    }
  }),
  columnHelper.accessor(row => (row.active ? "Active" : "Inactive"), {
    header: "Status",
    id: "active",
    filterFn: "equals",
    cell: info => <StatusCell info={info} />,
    meta: {
      filterType: "single-select"
    }
  })
];

export const UsersTable = () => {
  const { users } = useUsersContext();
  const tableInstance = useSortableReactTable<DatabaseUserValues>({
    data: users || [],
    columns: TABLE_COLUMNS,
    initialSorting: [
      {
        id: "user",
        desc: false
      }
    ]
  });

  return (
    <>
      <Stack>
        <FiltersList<DatabaseUserValues>
          columns={TABLE_COLUMNS}
          table={tableInstance}
        />
        <ReactTable<DatabaseUserValues>
          table={tableInstance}
          emptyStateMessage="No users found"
        />
      </Stack>
    </>
  );
};
