import { chain, map, without } from "lodash";
import React, { useContext, useEffect, useState } from "react";
import { Link, useRouteMatch } from "react-router-dom";
import { components, MultiValueGenericProps } from "react-select";
import useSWR from "swr";
import MultiSelect from "../../../components/pure-css/form/MultiSelect";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import { FormState, ViewContext } from "../constants";

const MultiValueLabel = (props: MultiValueGenericProps) => {
  const match = useRouteMatch();
  const to =
    match && match.url === "/"
      ? props.data.value
      : `${match.url}/${props.data.value}`;
  return (
    <Link
      to={to}
      style={{
        textDecoration: "underline"
      }}
    >
      <components.MultiValueLabel {...props} />
    </Link>
  );
};

const CollectionsDropdown = () => {
  const viewContext = useContext(ViewContext);
  const { dispatch, formState: fs, isAdminUser } = viewContext;
  const formState = fs as FormState;
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<{ label: string; value: string }[]>(
    []
  );
  useEffect(() => {
    if (data) {
      const collNames = map(data.body.result, "name");
      const tempOptions = collNames.sort().map(name => {
        return {
          label: name,
          value: name
        };
      });
      setOptions(tempOptions);
    }
  }, [data, formState.links]);

  const addLink = (link: string) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${link}]`,
        value: {}
      }
    });
    setOptions(
      without(options, {
        value: link,
        label: link
      })
    );
  };

  const removeLink = async (link: string) => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${link}]`,
        value: null
      }
    });
    setOptions(
      options
        .concat([
          {
            value: link,
            label: link
          }
        ])
        .sort()
    );
  };

  const validLinks = chain(formState.links)
    .toPairs()
    .filter(pair => pair[1] !== null)
    .map(pair => ({
      label: pair[0],
      value: pair[0]
    }))
    .value();

  return (
    <MultiSelect
      value={validLinks}
      options={options}
      components={{
        MultiValueLabel
      }}
      onChange={(_, action) => {
        if (action.action === "remove-value") {
          removeLink((action.removedValue as any).value as string);
          return;
        }
        if (action.action === "create-option") {
          addLink((action.option as any).value as string);
        }
      }}
    />
  );
};

export default CollectionsDropdown;
