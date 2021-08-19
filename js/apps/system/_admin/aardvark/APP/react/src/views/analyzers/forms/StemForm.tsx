import React from "react";
import { FormProps } from "../constants";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import { getPath } from "../helpers";

const StemForm = ({ formState, dispatch, disabled, basePath }: FormProps) => {
  const propertiesBasePath = getPath(basePath, 'properties');

  return <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch} disabled={disabled}
                   basePath={propertiesBasePath}/>
    </Cell>
  </Grid>;
};

export default StemForm;
