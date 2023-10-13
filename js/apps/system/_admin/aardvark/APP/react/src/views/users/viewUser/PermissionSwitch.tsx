import { Switch } from "@chakra-ui/react";
import React from "react";

export const PermissionSwitch = ({ checked }: { checked: boolean }) => {
  return <Switch isChecked={checked} />;
};
