import React, { useState, useContext, useRef, useEffect } from "react";

const LinkContext = React.createContext();
const ShowContext = React.createContext();
const FieldContext = React.createContext();
const FieldUpdateContext = React.createContext();
const ViewContext = React.createContext();
const UpdateViewContext = React.createContext();
const StoreLinksContext = React.createContext();
const UpdateLinksStoreContext = React.createContext();

export function useShow() {
  return useContext(LinkContext);
}

export function useShowUpdate() {
  return useContext(ShowContext);
}

export const useField = () => {
  return useContext(FieldContext);
}
export const useUpdateField = () => {
  return useContext(FieldUpdateContext);
}

export const useViewName = () => {
  return useContext(ViewContext);
}
export const useUpdateView = () => {
  return useContext(UpdateViewContext);
}

export const useLinks = () => {
  return useContext(StoreLinksContext);
}
export const useUpdateLinks = () => {
  return useContext(UpdateLinksStoreContext);
}

export const usePrevious = (value) => {
  const ref = useRef();
  useEffect(() => {
    ref.current = value;
  });
  return ref.current;
}


export function LinkProvider({ children }) {
  const [show, setShow] = useState("LinkList");
  const [field, setShowField] = useState({});
  const [view, setView] = useState("");
  const [links, setLinks] = useState("");


  const handleShowState = (str) => {
    setShow(str);
  }

  const handleShowField = (f) => {
    setShowField(f);
  }

  const handleView = (v) => {
    setView(v)
  }

  const handleStoreLinks = (l) => {
    setLinks(l);
  }
  return (
    <LinkContext.Provider value={show}>
      <ShowContext.Provider value={handleShowState}>
        <StoreLinksContext.Provider value={links}>
          <UpdateLinksStoreContext.Provider value={handleStoreLinks}>
            <FieldContext.Provider value={field}>
              <FieldUpdateContext.Provider value={handleShowField}>
                <ViewContext.Provider value={view}>
                  <UpdateViewContext.Provider value={handleView}>
                    {children}
                  </UpdateViewContext.Provider>
                </ViewContext.Provider>
              </FieldUpdateContext.Provider>
            </FieldContext.Provider>
          </UpdateLinksStoreContext.Provider>
        </StoreLinksContext.Provider>
      </ShowContext.Provider>
    </LinkContext.Provider>
  )
}
