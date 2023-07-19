import { Avatar, Link, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import _ from "lodash";
import React from "react";
import { Link as RouterLink } from "react-router-dom";
import { AccountCircle, Group } from "styled-icons/material";
import { DatabaseUserValues } from "../addUser/CreateUser.types";

export const StatusCell = ({
  info
}: {
  info: CellContext<DatabaseUserValues, string>;
}) => {
  const cellValue = info.cell.getValue();
  const isActive = cellValue === "Active";
  return (
    <Tag
      background={isActive ? "green.400" : "gray.200"}
      color={isActive ? "white" : ""}
    >
      {cellValue}
    </Tag>
  );
};

export const LinkCell = ({
  info
}: {
  info: CellContext<DatabaseUserValues, string>;
}) => {
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
};
export const AvatarCell = ({
  info
}: {
  info: CellContext<DatabaseUserValues, string>;
}) => {
  const isRole = info.row.original.user?.substring(0, 6) === ":role:";
  const hasImage = !_.isEmpty(info.row.original.extra.img);
  if (isRole) {
    return <Avatar size="xs" icon={<Group />} />;
  }
  if (hasImage) {
    return (
      <Avatar
        size="xs"
        src={`https://s.gravatar.com/avatar/${info.row.original.extra.img}`}
      />
    );
  }

  return <Avatar size="xs" icon={<AccountCircle />} />;
};
