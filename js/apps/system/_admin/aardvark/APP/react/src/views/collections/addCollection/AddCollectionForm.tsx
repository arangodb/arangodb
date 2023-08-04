import { Divider, Grid, Stack, Text } from "@chakra-ui/react";
import React from "react";
import { InputControl } from "../../../components/form/InputControl";
import { useCollectionsContext } from "../CollectionsContext";
import { useField } from "formik";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { useFetchDatabase } from "../../databases/useFetchDatabase";
import { getCurrentDB } from "../../../utils/arangoClient";
import { SelectControl } from "../../../components/form/SelectControl";
import { CreatableMultiSelectControl } from "../../../components/form/CreatableMultiSelectControl";

export const AddCollectionForm = () => {
  const { database, error } = useFetchDatabase(getCurrentDB().name);
  const { collections, isFormDisabled: isDisabled } = useCollectionsContext();
  const [distributeShardsLike] = useField<string>("distributeShardsLike");
  const [isSatellite] = useField<boolean>("isSatellite");
  if (error) {
    window.App.navigate("#collections", { trigger: true });
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${error.errorNum}: ${error.message}`
    );
  }
  if (!database) return null;
  const isOneShard = database.isOneShard || window.frontendConfig.forceOneShard;
  return (
    <Grid templateColumns={"1fr"} gap="6">
      <Stack>
        <Text>Basic</Text>
        <Divider borderColor="gray.400" />
        <Grid templateColumns={"1fr 1fr"} columnGap="4" alignItems="start">
          <InputControl isDisabled={isDisabled} name="name" label="Name" />
          <SelectControl
            isDisabled={isDisabled}
            name={"type"}
            label="Type"
            selectProps={{
              options: [
                { label: "Document", value: "document" },
                { label: "Edge", value: "edge" }
              ]
            }}
            tooltip={
              "Use the Document type to store json documents with unique _key attributes. Can be used as nodes in a graph. <br> Use the Edge type to store documents with special edge attributes (_to, _from), which represent relations. Can be used as edges in a graph."
            }
          />
        </Grid>
        <Text>Configuration</Text>
        <Divider borderColor="gray.400" />
        <Grid
          templateColumns={"1fr 1fr"}
          rowGap="5"
          columnGap="3"
          marginY="4"
          maxWidth="full"
        >
          {window.App.isCluster && (
            <>
              <InputControl
                isDisabled={
                  isDisabled ||
                  isOneShard ||
                  isSatellite.value ||
                  distributeShardsLike.value !== ""
                }
                name={"numberOfShards"}
                label="Number of shards"
                inputProps={{ type: "number" }}
                tooltip={
                  isOneShard
                    ? undefined
                    : `The number of shards to create. The maximum value is ${window.frontendConfig.maxNumberOfShards}. You cannot change this afterwards.`
                }
              />
              <CreatableMultiSelectControl
                isDisabled={isDisabled || isSatellite.value}
                name={"shardKeys"}
                label="Shard keys"
                selectProps={{
                  noOptionsMessage: () => "Start typing to add a key"
                }}
                tooltip={"The keys used to distribute documents on shards."}
              />
              <InputControl
                isDisabled={
                  isDisabled ||
                  isSatellite.value ||
                  distributeShardsLike.value !== ""
                }
                name={"replicationFactor"}
                label="Replication Factor"
                inputProps={{ type: "number" }}
                tooltip={"Total number of copies of the data in the cluster."}
              />
              <InputControl
                isDisabled={
                  isDisabled ||
                  isSatellite.value ||
                  distributeShardsLike.value !== ""
                }
                name={"writeConcern"}
                label="Write Concern"
                inputProps={{ type: "number" }}
                tooltip={
                  "Total number of copies of the data in the cluster that are required for each write operation. If we get below this value the collection will be read-only until enough copies are created. Must be smaller or equal compared to the replication factor."
                }
              />
              {window.frontendConfig.isEnterprise && (
                <>
                  {!isOneShard && (
                    <>
                      <SelectControl
                        isDisabled={isDisabled}
                        name={"distributeShardsLike"}
                        label="Distribute shards like"
                        selectProps={{
                          options: [
                            { label: "N/A", value: "" },
                            ...(collections?.map(collection => ({
                              label: collection.name,
                              value: collection.name
                            })) ?? [])
                          ]
                        }}
                        tooltip={
                          "Name of another collection that should be used as a prototype for sharding this collection."
                        }
                      />
                      <InputControl
                        isDisabled={isDisabled || isSatellite.value}
                        name={"smartJoinAttribute"}
                        label="SmartJoin attribute"
                        tooltip={
                          "String attribute name. Can be left empty if SmartJoins are not used."
                        }
                      />
                    </>
                  )}
                  <SwitchControl
                    isDisabled={isDisabled || distributeShardsLike.value !== ""}
                    name={"isSatellite"}
                    label="SatelliteCollection"
                    tooltip={
                      "If enabled, every collection in this collection will be replicated to every DB-Server."
                    }
                  />
                </>
              )}
            </>
          )}
          <SwitchControl
            isDisabled={isDisabled}
            name={"waitForSync"}
            label="Wait for sync"
            tooltip={
              "Synchronize to disk before returning from a create or update of a document."
            }
          />
        </Grid>
      </Stack>
    </Grid>
  );
};
