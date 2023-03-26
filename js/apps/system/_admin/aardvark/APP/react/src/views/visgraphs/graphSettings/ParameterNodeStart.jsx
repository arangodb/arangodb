import { FormLabel, Input } from "@chakra-ui/react";
import React, { useContext } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { UrlParametersContext } from "../url-parameters-context";

const ParameterNodeStart = () => {
  const [urlParameters, setUrlParameters] = useContext(UrlParametersContext);

  const handleChange = event => {
    const newUrlParameters = {
      ...urlParameters,
      nodeStart: event.target.value
    };
    setUrlParameters(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeStart"> Start node</FormLabel>
      <Input
        id="nodeStart"
        width="200px"
        min={1}
        required={true}
        value={urlParameters.nodeStart}
        background="#ffffff"
        size="sm"
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "A valid node ID or a space-separated list of IDs. If empty, a random node will be chosen."
        }
      />
    </>
  );
};

export default ParameterNodeStart;
