import React, { Dispatch } from "react";
import { DispatchArgs, FormProps } from "../../../utils/constants";
import CaseInput from "./inputs/CaseInput";
import LocaleInput from "./inputs/LocaleInput";
import { Cell, Grid } from "../../../components/pure-css/grid";
import AccentInput from "./inputs/AccentInput";
import { AccentProperty, CaseProperty, LocaleProperty, NormState } from "../constants";

const NormForm = ({ formState, dispatch, disabled }: FormProps<NormState>) =>
  <Grid>
    <Cell size={'1-2'}>
      <LocaleInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<LocaleProperty>>}
                   disabled={disabled}/>
    </Cell>

    <Cell size={'1-8'}>
      <CaseInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<CaseProperty>>}
                 disabled={disabled}/>
    </Cell>

    <Cell size={'1-8'}>
      <AccentInput formState={formState} dispatch={dispatch as Dispatch<DispatchArgs<AccentProperty>>}
                   disabled={disabled} inline={false}/>
    </Cell>
  </Grid>;

export default NormForm;
