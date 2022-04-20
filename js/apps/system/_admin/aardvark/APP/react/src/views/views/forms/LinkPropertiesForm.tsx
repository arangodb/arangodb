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
import JsonForm from "../forms/JsonForm";
type LinksPropertiesFormProps = FormProps<FormState> & {
  view: string;
};
const LinkPropertiesForm = ({
  disabled,
  view,
  formState: newFormstate
}: LinksPropertiesFormProps) => {
  const { formState, dispatch, field } = useContext(ViewContext);

  const [collection, setCollection, addDisabled, links] = useLinkState(
    formState,
    "links"
  );
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);
  const { show, setShow, setNewLink, newLink, link } = useContext(ViewContext);

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

  const getLink = (str: string) => {
    let formatedStr;
    let parentField;

    const replaceSquareBrackets = (
      str: string,
      idnt: string,
      index: number
    ) => {
      return str.split(idnt)[index].replace("]", "");
    };
    if (str !== "") {
      let link = replaceSquareBrackets(str, "[", 1);
      formatedStr = link;
      if (str.includes(".")) {
        link = replaceSquareBrackets(str.split(".")[0], "[", 1);
        parentField = replaceSquareBrackets(str.split(".")[1], "[", 1);
        formatedStr = `${link}/${parentField}`;
      } else {
        formatedStr = link;
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
    setNewLink(collection);
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
        <LinkView view={view} link={link} links={links} disabled={disabled} />
      )}

      {show === "JsonView" && (
        <JsonForm
          formState={newFormstate}
          dispatch={dispatch}
          renderKey={view}
          isEdit={true}
        />
      )}

      {show === "ViewChild" && (
        <LinkView
          view={view}
          link={newLink}
          links={links}
          disabled={disabled}
        />
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
