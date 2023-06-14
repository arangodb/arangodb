import { ChakraProvider } from '@chakra-ui/react';
import React from 'react';
import { theme } from '../../../theme/theme';
import { RebalanceShards } from './RebalanceShards';
import { ShardDistributionContent } from './ShardDistributionContent';

const ShardDistributionReactView = ({ readOnly }) => {
  const [refetchToken, setRefetchToken] = React.useState(() => Date.now());
  return (
    <ChakraProvider theme={theme}>
      <ShardDistributionContent refetchToken={refetchToken} />
      {!readOnly && <RebalanceShards refetchStats={() => setRefetchToken(Date.now())} />}
    </ChakraProvider>
  );
};

window.ShardDistributionReactView = ShardDistributionReactView;