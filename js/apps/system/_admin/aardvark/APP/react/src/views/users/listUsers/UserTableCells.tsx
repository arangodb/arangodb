import { Avatar, Icon, Link, Tag } from "@chakra-ui/react";
import { CellContext } from "@tanstack/react-table";
import _ from "lodash";
import React from "react";
import { AccountCircle, Group } from "styled-icons/material";
import { createEncodedUrl } from "../../../utils/urlHelper";
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
      background={isActive ? "green.600" : "gray.200"}
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
  const href = createEncodedUrl({
    path: "user",
    value: cellValue
  });
  return (
    <Link
      href={href}
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
    return <Icon as={Group} width="24px" height="24px" color="gray.400" />;
  }
  if (hasImage) {
    return (
      <Avatar
        size="xs"
        src={`https://s.gravatar.com/avatar/${info.row.original.extra.img}`}
      />
    );
  }

  return <Icon as={AccountCircle} width="24px" height="24px" color="gray.400" />;
};
