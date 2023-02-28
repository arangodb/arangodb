import Tag from "../../components/pure-css/form/Tag";

export const AttributesInfo = ({ attributes }) => {
  return (
    <div>
      {
        Object.keys(attributes)
        .map((key, i) => (
            <Tag label={`${key}: ${JSON.stringify(attributes[key])}`} />
        ))
      }
    </div>
  );
}
