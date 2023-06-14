import { Box, Button, FormLabel } from '@chakra-ui/react';
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
    if (res.body.result.moves.length === 0) {
      window.arangoHelper.arangoNotification('No move shard operations were performed.');
    } else {
      window.arangoHelper.arangoNotification('Rebalanced shards with ' + res.body.result.moves.length + ' move operation(s).');
    }
  } catch (e: any) {
    window.arangoHelper.arangoError(
      "Failure",
      `Got unexpected server response: ${e.message}`
    );
  }
}

export const RebalanceShards = () => {
  return (
    <Formik
      onSubmit={async ({includeSystemCollections, ...opts}) => {
        await rebalanceShards({
          excludeSystemCollections: !includeSystemCollections,
          ...opts
        });
      }}
      initialValues={{
        moveLeaders: false,
        moveFollowers: false,
        includeSystemCollections: false,
      }}
      isInitialValid={true}
    >{({isValid, isSubmitting}) => (
      <Form>
      <Global
        styles={{
          "label": {
            marginBottom: "unset"
          }
        }}
      />
        <Box width="100%" padding="5" background="white">
          <Box fontSize={"lg"}>
            Shard rebalancing
          </Box>
          <Box
            display={"grid"}
            gridTemplateColumns={"250px 40px 40px"}
            rowGap="5"
            columnGap="3"
            marginY="4"
            maxWidth="600px"
          >
            <FormLabel margin="0" htmlFor="moveLeaders">
              Move Leaders
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="moveLeaders"
            />
            <InfoTooltip
              label="Allow moving leaders."
              boxProps={{
                padding: "0",
                alignSelf: "start",
                top: "1"
              }}
            />
            <FormLabel margin="0" htmlFor="moveFollowers">
              Move Followers
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="moveFollowers"
            />
            <InfoTooltip
              label="Allow moving followers."
              boxProps={{
                padding: "0",
                alignSelf: "start",
                top: "1"
              }}
            />
            <FormLabel margin="0" htmlFor="includeSystemCollections">
              Include System Collections
            </FormLabel>
            <SwitchControl
              isDisabled={isSubmitting}
              name="includeSystemCollections"
            />
            <InfoTooltip
              label="Include system collections in the rebalance plan."
              boxProps={{
                padding: "0",
                alignSelf: "start",
                top: "1"
              }}
            />
          </Box>
          <Box
          >
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
        </Box>
      </Form>
      )}
    </Formik>
  );
};
