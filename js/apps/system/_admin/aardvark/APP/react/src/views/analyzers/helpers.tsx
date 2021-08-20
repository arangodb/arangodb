import Ajv from "ajv";
import { AnalyzerTypeState, FormProps, formSchema } from "./constants";
import DelimiterForm from "./forms/DelimiterForm";
import StemForm from "./forms/StemForm";
import NormForm from "./forms/NormForm";
import NGramForm from "./forms/NGramForm";
import TextForm from "./forms/TextForm";
import AqlForm from "./forms/AqlForm";
import GeoJsonForm from "./forms/GeoJsonForm";
import GeoPointForm from "./forms/GeoPointForm";
import React from "react";
import PipelineForm from "./forms/PipelineForm";
import { compact } from "lodash";

const ajv = new Ajv({
  removeAdditional: 'failing',
  useDefaults: true,
  discriminator: true,
  $data: true
});
export const validateAndFix = ajv.compile(formSchema);

export const getPath = (basePath: string | undefined, path: string | undefined) => compact([basePath, path]).join('.');

export const getForm = ({ formState, dispatch, disabled = true }: FormProps) => {
  switch ((formState as AnalyzerTypeState).type) {
    case 'identity':
      return null;

    case 'delimiter':
      return <DelimiterForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'stem':
      return <StemForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'norm':
      return <NormForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'ngram':
      return <NGramForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'text':
      return <TextForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'aql':
      return <AqlForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'geojson':
      return <GeoJsonForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'geopoint':
      return <GeoPointForm formState={formState} dispatch={dispatch} disabled={disabled}/>;

    case 'pipeline':
      return <PipelineForm formState={formState} dispatch={dispatch} disabled={disabled}/>;
  }
};
