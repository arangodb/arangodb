import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useContext, useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterNodeColorByCollection = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);
  const [nodeColorByCollection, setNodeColorByCollection] = useState(
    urlParameters.nodeColorByCollection
  );

  const handleChange = event => {
    setNodeColorByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParameters,
      nodeColorByCollection: event.target.checked
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeColorByCollection">
        Color nodes by collection
      </FormLabel>
      <Checkbox
        id="nodeColorByCollection"
        checked={nodeColorByCollection}
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "Should nodes be colorized by their collection? If enabled, node color and node color attribute will be ignored."
        }
      />
    </>
  );
};

export default ParameterNodeColorByCollection;
