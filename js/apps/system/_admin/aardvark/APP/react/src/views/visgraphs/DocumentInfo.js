import Tag from "../../components/pure-css/form/Tag";

export const DocumentInfo = ({ attributes }) => {

  return (
    <div>
      {
        Object.keys(attributes)
        .map((key, i) => (
            <Tag key={key.toString()} label={`${key}: ${JSON.stringify(attributes[key])}`}/>
        ))
      }
    </div>
  );
}
