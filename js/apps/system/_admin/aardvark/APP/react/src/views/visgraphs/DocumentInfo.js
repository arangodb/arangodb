import {
  Box,
  VStack,
  Tag } from '@chakra-ui/react';

export const DocumentInfo = ({ attributes }) => {

  return (
    <Box width={'97%'} margin='auto' p='2'>
      <VStack spacing={2} alignItems="start">
        {
          Object.keys(attributes)
          .map((key, i) => (
            <Tag size={'md'} key={'nodesCountTag'} variant='solid' background='gray.800' color='white'>
              {`${key}: ${JSON.stringify(attributes[key])}`}
            </Tag>
          ))
        }
      </VStack>
    </Box>
  );
}
