import {
  Divider,
  FormLabel,
  Grid,
  Stack,
  Text,
  VStack
} from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { useDatabasesContext } from "../DatabasesContext";
import { useField } from "formik";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { MultiSelectControl } from "../../../components/form/MultiSelectControl";
import { useFetchUsers } from "../../users/useFetchUsers";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";

export const AddDatabaseForm = ({
  initialFocusRef
}: {
  initialFocusRef?: React.RefObject<HTMLInputElement>;
}) => {
  const { users: availableUsers = [] } = useFetchUsers();
  const { isFormDisabled: isDisabled } = useDatabasesContext();
  const [isSatellite] = useField<boolean>("isSatellite");
  const [path] = useField<string>("path");
  const [users] = useField<string[] | undefined>("users");
  const showUsers = Boolean(users.value);
  const isUsersDisabled = Boolean(
    !availableUsers.length ||
      (availableUsers.length === 1 && availableUsers[0].user === "root")
  );
  return (
    <Grid templateColumns={"1fr"} gap="6">
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
      <Stack hidden={!window.App.isCluster && !showUsers}>
        <Text>Configuration</Text>
        <Divider borderColor="gray.400" />
        <VStack spacing={4} align="stretch">
          <Grid
            templateColumns={"200px 1fr 40px 200px 1fr 40px"}
            rowGap="5"
            columnGap="3"
            maxWidth="full"
            alignItems="center"
          >
            {window.App.isCluster && (
              <>
                <FormLabel margin="0" htmlFor={"replicationFactor"}>
                  Replication Factor
                </FormLabel>
                <InputControl
                  isDisabled={isDisabled || isSatellite.value}
                  name={"replicationFactor"}
                  inputProps={{
                    type: "number",
                    min: 1
                  }}
                />
                <InfoTooltip
                  label={
                    "Default replication factor for new collections created in this database. For SatelliteCollections, the replication factor is automatically controlled to equal the number of DB-Servers."
                  }
                />
                <FormLabel margin="0" htmlFor={"writeConcern"}>
                  Write Concern
                </FormLabel>
                <InputControl
                  isDisabled={isDisabled || isSatellite.value}
                  name={"writeConcern"}
                  inputProps={{
                    type: "number",
                    min: 1
                  }}
                />
                <InfoTooltip
                  label={
                    "Default write concern for new collections created in this database. It determines how many copies of each shard are required to be in sync on the different DB-Servers. If there are less than these many copies in the cluster, a shard refuses to write. Writes to shards with enough up-to-date copies succeed at the same time, however. The value of writeConcern cannot be greater than replicationFactor. For SatelliteCollections, the writeConcern is automatically controlled to equal the number of DB-Servers."
                  }
                />
                {Boolean(
                  window.frontendConfig.isEnterprise &&
                    !window.frontendConfig.forceOneShard
                ) && (
                  <>
                    <FormLabel margin="0" htmlFor={"isOneShard"}>
                      OneShard
                    </FormLabel>
                    <SwitchControl
                      isDisabled={
                        isDisabled || window.frontendConfig.forceOneShard
                      }
                      name={"isOneShard"}
                      switchProps={{
                        isChecked:
                          window.frontendConfig.forceOneShard || undefined
                      }}
                    />
                    <InfoTooltip
                      label={
                        "If enabled, all collections in this database will be restricted to a single shard and placed on a single DB-Server."
                      }
                    />
                  </>
                )}
                {window.frontendConfig.isEnterprise && (
                  <>
                    <FormLabel margin="0" htmlFor={"isSatellite"}>
                      Satellite
                    </FormLabel>
                    <SwitchControl
                      isDisabled={isDisabled}
                      name={"isSatellite"}
                    />
                    <InfoTooltip
                      label={
                        "If enabled, every collection in this database will be replicated to every DB-Server."
                      }
                    />
                  </>
                )}
              </>
            )}
            {showUsers && (
              <>
                <FormLabel margin="0" htmlFor={"users"}>
                  Users
                </FormLabel>
                <MultiSelectControl
                  isDisabled={isDisabled || isUsersDisabled}
                  name={"users"}
                  selectProps={{
                    placeholder: "root",
                    options: availableUsers.map(user => ({
                      label: user.extra?.name || user.user,
                      value: user.user
                    }))
                  }}
                />
                <InfoTooltip
                  label={
                    "Users that will be granted Administrate permissions for the new database. If no users are selected, the default user root will be used to ensure that the new database will be accessible after it is created."
                  }
                />
              </>
            )}
          </Grid>
        </VStack>
      </Stack>
    </Grid>
  );
};
