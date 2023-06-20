import { Box, Button, FormLabel, Grid, Text } from '@chakra-ui/react';
import { Global } from '@emotion/react';
import { Form, Formik } from 'formik';
import React from 'react';
import { SwitchControl } from '../../../components/form/SwitchControl';
import { InfoTooltip } from '../../../components/tooltip/InfoTooltip';
import { getAdminRouteForCurrentDB } from '../../../utils/arangoClient';

async function rebalanceShards(opts: {
  maximumNumberOfMoves?: number;
  leaderChanges?: boolean;
  moveLeaders?: boolean;
  moveFollowers?: boolean;
  excludeSystemCollections?: boolean;
  piFactor?: number;
  databasesExcluded?: string[]
}) {
  const route = getAdminRouteForCurrentDB();
  try {
    const res = await route.put("cluster/rebalance", {
      version: 1,
      ...opts
    });
    const result = res.body.result;
    if (result.moves.length === 0) {
      window.arangoHelper.arangoNotification('No move shard operations were performed.');
    } else {
      window.arangoHelper.arangoNotification(`Rebalanced shards with ${result.moves.length} move operation(s).`);
    }
    return result;
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.message}`
    );
  }
}

export const RebalanceShards = ({ refetchStats }: { refetchStats: () => void }) => {
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
        includeSystemCollections: false,
      }}
      isInitialValid={true}
    >{({ isValid, isSubmitting }) => (
      <Form>
        <Global
          styles={{
            "label": {
              marginBottom: "unset"
            }
          }}
        />
        <Box width="100%" padding="5" background="white">
          <Text fontSize={"lg"}>
            Shard rebalancing
          </Text>
          <Grid
            gridTemplateColumns={"250px 40px 40px"}
            rowGap="5"
            columnGap="3"
            marginY="4"
            maxWidth="600px"
            alignItems="center"
          >
            <FormLabel margin="0" htmlFor="moveLeaders">
              Move Leaders
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="moveLeaders"
            />
            <InfoTooltip label="Allow moving leaders." />
            <FormLabel margin="0" htmlFor="moveFollowers">
              Move Followers
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="moveFollowers"
            />
            <InfoTooltip label="Allow moving followers." />
            <FormLabel margin="0" htmlFor="includeSystemCollections">
              Include System Collections
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="includeSystemCollections"
            />
            <InfoTooltip label="Include system collections in the rebalance plan." />
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
        </Box>
      </Form>
    )}
    </Formik>
  );
};
