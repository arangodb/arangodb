import { Button, FormLabel, Grid, Text } from "@chakra-ui/react";
import { Global } from "@emotion/react";
import { Form, Formik } from "formik";
import React from "react";
import { SwitchControl } from "../../../components/form/SwitchControl";
import { getAdminRouteForCurrentDB } from "../../../utils/arangoClient";

async function rebalanceShards(opts: {
  maximumNumberOfMoves?: number;
  leaderChanges?: boolean;
  moveLeaders?: boolean;
  moveFollowers?: boolean;
  excludeSystemCollections?: boolean;
  piFactor?: number;
  databasesExcluded?: string[];
}) {
  const route = getAdminRouteForCurrentDB();
  try {
    const res = await route.put("cluster/rebalance", {
      version: 1,
      ...opts
    });
    const result = res.parsedBody.result;
    if (result.moves.length === 0) {
      window.arangoHelper.arangoNotification(
        "No move shard operations were performed."
      );
    } else {
      window.arangoHelper.arangoNotification(
        `Rebalanced shards with ${result.moves.length} move operation(s).`
      );
    }
    return result;
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.message}`
    );
  }
}

export const RebalanceShards = ({
  refetchStats
}: {
  refetchStats: () => void;
}) => {
  return (
    <Formik
      onSubmit={async ({ includeSystemCollections, ...opts }) => {
        const result = await rebalanceShards({
          excludeSystemCollections: !includeSystemCollections,
          ...opts
        });
        if (result && result.moves.length > 0) refetchStats();
      }}
      initialValues={{
        moveLeaders: false,
        moveFollowers: false,
        includeSystemCollections: false
      }}
      isInitialValid={true}
    >
      {({ isValid, isSubmitting }) => (
        <Form>
          <Global
            styles={{
              label: {
                marginBottom: "unset"
              }
            }}
          />
          <Text fontSize={"lg"} marginY="8">
            Shard rebalancing
          </Text>
          <Text>
            By default, rebalancing changes leaders only without moving data.
            <br />
            To correct data imbalance and move leader and/or follower shards,
            use the options below.
          </Text>
          <Grid
            gridTemplateColumns={"250px 1fr"}
            rowGap="5"
            columnGap="3"
            marginY="4"
            maxWidth="600px"
            alignItems="center"
          >
            <FormLabel margin="0" htmlFor="moveLeaders">
              Move Leader Shards
            </FormLabel>
            <SwitchControl isDisabled={isSubmitting} name="moveLeaders" />
            <FormLabel margin="0" htmlFor="moveFollowers">
              Move Follower Shards
            </FormLabel>
            <SwitchControl isDisabled={isSubmitting} name="moveFollowers" />
            <FormLabel margin="0" htmlFor="includeSystemCollections">
              Include System Collections
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="includeSystemCollections"
            />
          </Grid>
          <Button
            colorScheme="green"
            size="sm"
            type="submit"
            isDisabled={!isValid}
            isLoading={isSubmitting}
          >
            Rebalance
          </Button>
        </Form>
      )}
    </Formik>
  );
};
