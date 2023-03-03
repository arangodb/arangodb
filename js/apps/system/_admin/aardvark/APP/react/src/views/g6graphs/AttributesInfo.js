import { Tag } from 'antd';

export const AttributesInfo = ({ attributes }) => {
  return (
    <div>
      {
        Object.keys(attributes)
        .map((key, i) => (
            <Tag color="cyan" key={key.toString()}><strong>{key}:</strong> {JSON.stringify(attributes[key])}</Tag>
        ))
      }
    </div>
  );
}
