import React, { useContext, useState } from "react";
import { UrlParametersContext } from "./url-parameters-context";
import { Checkbox, Tooltip } from 'antd';
import { InfoCircleFilled } from '@ant-design/icons';

const ParameterEdgeLabelByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [edgeLabelByCollection, setEdgeLabelByCollection] = useState(urlParameters.edgeLabelByCollection);

  const NEWURLPARAMETERS = { ...urlParameters };
  
  return (
    <div>
      <Checkbox
        checked={edgeLabelByCollection}
        onChange={() => {
          const newEdgeLabelByCollection = !edgeLabelByCollection;
          setEdgeLabelByCollection(newEdgeLabelByCollection);
          NEWURLPARAMETERS.edgeLabelByCollection = newEdgeLabelByCollection;
          setUrlParameters(NEWURLPARAMETERS);
        }}>
        Show collection name
      </Checkbox>
      <Tooltip placement="bottom" title={"Append collection name to the label?"}>
        <InfoCircleFilled style={{ fontSize: '12px', color: '#555555' }} />
      </Tooltip>
      <p>Do we show the collection name? {edgeLabelByCollection.toString()}</p>
    </div>
  );
};

export default ParameterEdgeLabelByCollection;
