import React, { useState, useEffect } from "react";
import { FormProps } from "../../../utils/constants";
import { FormState } from "../constants";
import { useLinkState } from "../helpers";
import { map, chain, isNull, difference, isEmpty } from "lodash";
import NewLink from "../Components/NewLink";
import ViewLayout from "../Components/ViewLayout";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";

const NewLinkInput = ({
  formState,
  dispatch,
  disabled,
  view
}: FormProps<FormState>) => {
  const [collection, setCollection, addDisabled, links] = useLinkState(
    formState,
    "links"
  );
  debugger;
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );

  const [options, setOptions] = useState<string[]>([]);

  useEffect(() => {
    if (data) {
      const linkKeys = chain(links)
        .omitBy(isNull)
        .keys()
        .value();
      const collNames = map(data.body.result, "name");
      const tempOptions = difference(collNames, linkKeys).sort();

      setOptions(tempOptions);
    }
  }, [data, links]);

  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    setCollection("");
  };

  const updateCollection = (value: string | number) => {
    setCollection(value);
  };
  return (
    <>
      {!disabled && isEmpty(links) && (
        <ViewLayout view={view} disabled={disabled}>
          <NewLink
            disabled={addDisabled || !options.includes(collection)}
            addLink={addLink}
            collection={collection}
            updateCollection={updateCollection}
            options={options}
          />
        </ViewLayout>
      )}
    </>
  );
};

export default NewLinkInput;
