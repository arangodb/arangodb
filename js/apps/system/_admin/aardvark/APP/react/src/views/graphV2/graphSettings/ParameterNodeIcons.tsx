import { Checkbox, FormLabel } from "@chakra-ui/react";
import React, { ChangeEvent } from "react";
import { InfoTooltip } from "../../../components/tooltip/InfoTooltip";
import { useUrlParameterContext } from "../UrlParametersContext";

const ParameterNodeIcons = () => {
  const { urlParams, setUrlParams } = useUrlParameterContext();

  const handleChange = (event: ChangeEvent<HTMLInputElement>) => {
    const newUrlParameters = {
      ...urlParams,
      nodeIcons: event.target.checked
    };
    setUrlParams(newUrlParameters);
  };

  return (
    <>
      <FormLabel htmlFor="nodeColorByCollection">
        Use icons
      </FormLabel>
      <Checkbox
        id="nodeIcons"
        isChecked={urlParams.nodeIcons}
        onChange={handleChange}
      />
      <InfoTooltip
        label={
          "Nodes will be displayed by your set icons for the vertex collections."
        }
      />
    </>
  );
};

export default ParameterNodeIcons;
