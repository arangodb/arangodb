import { useEffect, useState } from "react"

export const useNavbarHeight = () => {
  const navbar = document.getElementById("navbar2");
  const [height, setHeight] = useState(navbar ? navbar.getBoundingClientRect().height : 0);
  console.log('height', height)
  useEffect(() => {
    if (!navbar) return;
    const callback = () => {
      setHeight(navbar.getBoundingClientRect().height);
    };
    const observer = new MutationObserver(callback);
    observer.observe(navbar, { childList: true, subtree: true });
    return () => observer.disconnect();
  }, [navbar]);
  return height;
}