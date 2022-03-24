import { FormProps } from "../../../utils/constants";
import { FormState } from "../constants";
import React, { useContext, useEffect, useState } from "react";
import { chain, difference, isEmpty, isNull, map, noop } from "lodash";
import { useLinkState } from "../helpers";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import NewLink from "../Components/NewLink";
import LinkView from "../Components/LinkView";
import FieldView from "../Components/FieldView";
import { ViewContext } from "../ViewLinksReactView";

const LinkPropertiesForm = ({ disabled, view }: FormProps<FormState>) => {
  const { formState, dispatch, field } = useContext(ViewContext);

  const [collection, setCollection, addDisabled, links] = useLinkState(
    formState,
    "links"
  );

  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);
  const [link, setLink] = useState<string>();
  const { show, setShow } = useContext(ViewContext);

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

  const linkVal = chain(links)
    .omitBy(isNull)
    .keys()
    .value()[0];

  const getLink = (str: string) => {
    console.log(str);
    let formatedStr;
    if (str !== "") {
      const toReturn = str.split("[")[1].replace("]", "");
      if (toReturn.includes(".")) {
        formatedStr = toReturn.split(".")[0];
      } else {
        formatedStr = toReturn;
      }
    }
    return formatedStr;
  };

  const addLink = () => {
    dispatch({
      type: "setField",
      field: {
        path: `links[${collection}]`,
        value: {}
      }
    });
    setCollection("");
    setShow("ViewChild");
    setLink(collection);
  };

  return disabled && isEmpty(links) ? (
    <span>No links found.</span>
  ) : (
    <main>
      {!disabled && show === "AddNew" && (
        <NewLink
          view={view}
          disabled={addDisabled || !options.includes(collection)}
          addLink={addLink}
          collection={collection}
          updateCollection={setCollection}
          options={options}
        />
      )}

      {show === "ViewParent" && (
        <LinkView
          view={view}
          link={linkVal}
          links={links}
          disabled={disabled}
        />
      )}

      {show === "ViewChild" && (
        <LinkView view={view} link={link} links={links} disabled={disabled} />
      )}

      {show === "ViewField" && (
        <FieldView
          view={view}
          disabled={disabled}
          basePath={field.basePath}
          viewField={noop}
          fieldName={field.field}
          link={getLink(field.basePath)}
        />
      )}
    </main>
  );
};

export default LinkPropertiesForm;
