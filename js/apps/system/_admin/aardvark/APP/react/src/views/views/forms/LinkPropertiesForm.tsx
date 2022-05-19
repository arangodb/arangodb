import React, { useContext, useEffect, useState } from "react";
import { chain, difference, get, isEmpty, isNull, map, noop } from "lodash";
import { useLinkState } from "../helpers";
import useSWR from "swr";
import { getApiRouteForCurrentDB } from "../../../utils/arangoClient";
import NewLink from "../Components/NewLink";
import LinkView from "../Components/LinkView";
import FieldView from "../Components/FieldView";
import { Route, Switch, useRouteMatch } from "react-router-dom";
import { BackButton, SaveButton } from "../Actions";
import { FormState, ViewContext } from "../constants";

type LinksPropertiesFormProps = {
  disabled?: boolean;
};
const LinkPropertiesForm = ({ disabled }: LinksPropertiesFormProps) => {
  const { formState, isAdminUser } = useContext(ViewContext);
  const match = useRouteMatch();
  const [collection, setCollection, addDisabled, links] = useLinkState(
     formState,
    "links"
  );
  const { data } = useSWR(["/collection", "excludeSystem=true"], (path, qs) =>
    getApiRouteForCurrentDB().get(path, qs)
  );
  const [options, setOptions] = useState<string[]>([]);

  const link = get(match.params, 'link');

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

  return <div
    id={'modal-dialog'}
    className={'createModalDialog'}
    tabIndex={-1}
    role={'dialog'}
    aria-labelledby={'myModalLabel'}
    aria-hidden={'true'}
  >
    <div className="modal-body" style={{ overflowY: 'visible' }}>
      <div className={'tab-content'}>
        <div className="tab-pane tab-pane-modal active" id="Links">
          {
            disabled && isEmpty(links)
              ? <span>No links found.</span>
              : <main>
                <Switch>
                  <Route path={`${match.path}`}>
                    <LinkView link={link} links={links} disabled={disabled}/>
                  </Route>
                  <Route path={`${match.path}/_add`}>
                    {
                      disabled
                        ? null
                        : <NewLink
                          disabled={addDisabled || !options.includes(collection)}
                          collection={collection}
                          updateCollection={setCollection}
                          options={options}
                        />
                    }
                  </Route>
                  <Route path={`${match.path}/_new`}>
                    <LinkView
                      link={'newLink'}
                      links={links}
                      disabled={disabled}
                    />
                  </Route>
                  <Route path={`${match.path}/:field`}>
                    <FieldView
                      disabled={disabled}
                      basePath={'field.basePath'}
                      viewField={noop}
                      fieldName={'field.field'}
                      link={getLink('field.basePath')}
                    />
                  </Route>
                </Switch>
              </main>
          }
        </div>
      </div>
    </div>
     <div className="modal-footer">
      <BackButton disabled={false /* !!newLink */}/>
      {
        isAdminUser
          ? <SaveButton view={formState as FormState}/>
          : null
      }
     </div>
  </div>;
};

export default LinkPropertiesForm;
