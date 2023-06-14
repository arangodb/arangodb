import { ChakraProvider } from '@chakra-ui/react';
import React from 'react';
import { theme } from '../../../theme/theme';
import { RebalanceShards } from './RebalanceShards';
import { ShardDistributionContent } from './ShardDistributionContent';

const ShardDistributionReactView = ({ readOnly }) => {
  return (
    <ChakraProvider theme={theme}>
      <ShardDistributionContent />
      {!readOnly && <RebalanceShards />}
    </ChakraProvider>
  );
};

window.ShardDistributionReactView = ShardDistributionReactView;