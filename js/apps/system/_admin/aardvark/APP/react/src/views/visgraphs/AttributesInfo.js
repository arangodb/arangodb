import { Tag } from '@chakra-ui/react';

export const AttributesInfo = ({ attributes }) => {
  return (
    <div>
      {
        Object.keys(attributes)
        .map((key, i) => (
            <Tag>{`${key}: ${JSON.stringify(attributes[key])}`}</Tag>
        ))
      }
    </div>
  );
}
