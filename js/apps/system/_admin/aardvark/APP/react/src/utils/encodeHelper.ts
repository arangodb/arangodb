export const encodeHelper = (value?: string) => {
  const normalized = value?.normalize();
  const encoded = normalized ? encodeURIComponent(normalized) : "";
  return { encoded, normalized };
};
