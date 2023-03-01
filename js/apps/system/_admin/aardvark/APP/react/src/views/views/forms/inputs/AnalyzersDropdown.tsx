import { get, without } from "lodash";
import React, { useContext, useEffect, useMemo, useState } from "react";
import useSWR from "swr";
import MultiSelect, {
  OptionType
} from "../../../../components/pure-css/form/MultiSelect";
import { getApiRouteForCurrentDB } from "../../../../utils/arangoClient";
import { LinkProperties, ViewContext } from "../../constants";

export const AnalyzersDropdown = ({
  basePath,
  isDisabled,
  formState
}: {
  basePath: string;
  isDisabled: boolean;
  formState: LinkProperties;
}) => {
  const viewContext = useContext(ViewContext);
  const { dispatch } = viewContext;
  const { data } = useSWR("/analyzer", path =>
    getApiRouteForCurrentDB().get(path)
  );
  const [options, setOptions] = useState<OptionType[]>([]);
  const analyzers = useMemo(() => get(formState, "analyzers", [] as string[]), [
    formState
  ]);

  useEffect(() => {
    if (data) {
      const tempOptions = (data.body.result as { name: string }[]).map(
        option => {
          return {
            value: option.name,
            label: option.name
          };
        }
      );

      setOptions(tempOptions);
    }
  }, [data]);

  const addAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: analyzers.concat([analyzer as string])
      },
      basePath
    });
    setOptions(
      options.filter(option => {
        return option.value === analyzer;
      })
    );
  };

  const removeAnalyzer = (analyzer: string | number) => {
    dispatch({
      type: "setField",
      field: {
        path: "analyzers",
        value: without(analyzers, analyzer)
      },
      basePath
    });
    setOptions(
      options
        .concat([{ value: analyzer.toString(), label: analyzer.toString() }])
        .sort()
    );
  };
  const newAnalyzers = analyzers.map(analyzer => {
    return { value: analyzer, label: analyzer };
  });
  return (
    <MultiSelect
      value={newAnalyzers}
      options={options}
      noOptionsMessage={() => "Analyzer does not exist."}
      placeholder={"Start typing for suggestions."}
      styles={{
        option: baseStyles => ({
          ...baseStyles,
          overflow: "hidden",
          textOverflow: "ellipsis"
        })
      }}
      isDisabled={isDisabled}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeAnalyzer((action.removedValue as any).value as string);
          return;
        }
        if (action.action === "select-option") {
          addAnalyzer((action.option as any).value as string);
        }
      }}
    />
  );
};

export default AnalyzersDropdown;
