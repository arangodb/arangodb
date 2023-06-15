import { ChakraProvider } from '@chakra-ui/react';
import React from 'react';
import { theme } from '../../../theme/theme';
import { LegacyShardDistributionContent } from './LegacyShardDistributionContent';
import { RebalanceShards } from './RebalanceShards';

const ShardDistributionReactView = ({ readOnly }: { readOnly: boolean }) => {
  const [refetchToken, setRefetchToken] = React.useState(() => Date.now());
  return (
    <ChakraProvider theme={theme}>
      <LegacyShardDistributionContent refetchToken={refetchToken} />
      {!readOnly && <RebalanceShards refetchStats={() => setRefetchToken(Date.now())} />}
    </ChakraProvider>
  );
};

(window as any).ShardDistributionReactView = ShardDistributionReactView;