import { Avatar, Link, Stack, Tag } from "@chakra-ui/react";
import { createColumnHelper } from "@tanstack/react-table";
import _ from "lodash";
import React from "react";
import { Link as RouterLink } from "react-router-dom";
import { FiltersList } from "../../../components/table/FiltersList";
import { ReactTable } from "../../../components/table/ReactTable";
import { useSortableReactTable } from "../../../components/table/useSortableReactTable";
import { DatabaseUserValues } from "../addUser/CreateUser.types";
import { useUsersContext } from "../UsersContext";
import { AccountCircle, Group } from "styled-icons/material";

const columnHelper = createColumnHelper<DatabaseUserValues>();

const TABLE_COLUMNS = [
  columnHelper.accessor("user", {
    header: "Username",
    id: "user",
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
  }),
  columnHelper.accessor("extra.name", {
    header: "Name",
    id: "extra.name",
    cell: info => {
      const cellValue = info.cell.getValue();
      return <>{cellValue}</>;
    },
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
      if (info.row.original.user?.substring(0, 6) === ":role:") {
        return <Avatar size="xs" icon={<Group />} />;
      }

      if (!_.isEmpty(info.row.original.extra.img)) {
        return (
          <Avatar
            size="xs"
            src={`https://s.gravatar.com/avatar/${info.row.original.extra.img}`}
          />
        );
      }

      return <Avatar size="xs" icon={<AccountCircle />} />;
    }
  }),
  columnHelper.accessor(row => (row.active ? "Active" : "Inactive"), {
    header: "Active",
    id: "active",
    filterFn: "equals",
    cell: info => {
      const cellValue = info.cell.getValue();
      return (
        <>
          {cellValue === "Active" ? (
            <Tag background="green.400" color="white">
              {cellValue}
            </Tag>
          ) : (
            <Tag>{cellValue}</Tag>
          )}
        </>
      );
    },
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
