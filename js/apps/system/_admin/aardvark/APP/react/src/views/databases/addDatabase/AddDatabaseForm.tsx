import { Divider, Grid, Stack, Text } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { useDatabasesContext } from "../DatabasesContext";
import { useField } from "formik";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { MultiSelectControl } from "../../../components/form/MultiSelectControl";
import { useFetchUsers } from "../../users/useFetchUsers";
export const AddDatabaseForm = ({
  initialFocusRef
}: {
  initialFocusRef?: React.RefObject<HTMLInputElement>;
}) => {
  const { users: knownUsers = [] } = useFetchUsers();
  const { isFormDisabled: isDisabled } = useDatabasesContext();
  const [isSatellite] = useField<boolean>("isSatellite");
  const [path] = useField<string>("path");
  const [users] = useField<string[] | undefined>("users");
  const availableUsers = knownUsers.filter(user => user.user !== "root");
  const showUsers = Boolean(users.value && availableUsers.length);
  return (
    <Grid templateColumns={"1fr 1fr"} gap="6">
      <Stack>
        <Text>Basic</Text>
        <Divider borderColor="gray.400" />
        <Grid templateColumns={"1fr 1fr"} columnGap="4" alignItems="start">
          <InputControl
            ref={initialFocusRef}
            isDisabled={isDisabled}
            name="name"
            label="Database name"
          />
          <InputControl
            isDisabled
            name="path"
            label="Path"
            hidden={!path.value}
          />
        </Grid>
      </Stack>
      <Stack gridColumn="1 / -1" hidden={!window.App.isCluster && !showUsers}>
        <Text>Configuration</Text>
        <Divider borderColor="gray.400" />
        <Grid templateColumns={"1fr 1fr 1fr"} columnGap="4">
          <InputControl
            isDisabled={isDisabled || isSatellite.value}
            name={"replicationFactor"}
            label={"Replication Factor"}
            hidden={!window.App.isCluster}
            inputProps={{
              type: "number",
              min: 1
            }}
          />
          <InputControl
            isDisabled={isDisabled}
            name={"writeConcern"}
            label={"Write Concern"}
            hidden={!window.App.isCluster}
            inputProps={{
              type: "number",
              min: 1
            }}
          />
          <SwitchControl
            isDisabled={isDisabled || window.frontendConfig.forceOneShard}
            name={"isOneShard"}
            label={"OneShard"}
            hidden={
              !window.App.isCluster ||
              !window.frontendConfig.isEnterprise ||
              window.frontendConfig.forceOneShard
            }
            switchProps={{
              isChecked: window.frontendConfig.forceOneShard || undefined
            }}
          />
          <SwitchControl
            isDisabled={isDisabled}
            name={"isSatellite"}
            label={"Satellite"}
            hidden={!window.App.isCluster || isSatellite.value === undefined}
          />
          <MultiSelectControl
            isDisabled={isDisabled || !availableUsers.length}
            name={"users"}
            label="Users"
            hidden={!showUsers}
            selectProps={{
              options: availableUsers.map(user => ({
                label: user.extra?.name || user.user,
                value: user.user
              }))
            }}
          />
        </Grid>
      </Stack>
    </Grid>
  );
};
