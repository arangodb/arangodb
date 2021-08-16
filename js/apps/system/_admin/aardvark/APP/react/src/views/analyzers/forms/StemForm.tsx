import React from "react";
import { FormProps, StemState } from "../constants";
import LocaleInput from "./LocaleInput";

const StemForm = ({ state, dispatch }: FormProps) => {
  const formState = state.formState as StemState;

  return <div className={'pure-g'}>
    <div className={'pure-u-12-24 pure-u-md-12-24 pure-u-lg-12-24 pure-u-xl-12-24'}>
      <LocaleInput formState={formState} dispatch={dispatch}/>
    </div>
  </div>;
};

export default StemForm;
