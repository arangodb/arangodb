import { Tag } from 'antd';

export const AttributesInfo = ({ attributes }) => {
  return (
    <>
      {
      Object.keys(attributes)
        .map((key) => <Tag color="cyan"><strong>{key}</strong></Tag>)
      }
    </>
  );
}
