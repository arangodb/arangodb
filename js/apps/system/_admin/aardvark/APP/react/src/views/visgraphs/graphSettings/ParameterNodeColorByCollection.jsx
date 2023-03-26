import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { useState } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeColorByCollection = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();
  const [nodeColorByCollection, setNodeColorByCollection] = useState(
    urlParams.nodeColorByCollection
  );

  const handleChange = event => {
    setNodeColorByCollection(event.target.checked);
    const newUrlParameters = {
      ...urlParams,
      nodeColorByCollection: event.target.checked
    };
    setUrlParams(newUrlParameters);
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
