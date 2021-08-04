export const typeNameMap = {
  identity: 'Identity',
  delimiter: 'Delimiter',
  stem: 'Stem',
  norm: 'Norm',
  ngram: 'N-Gram',
  text: 'Text'
};

export interface FormProps {
  formState: { [key: string]: any };
  updateFormField: (field: string, value: any) => void;
}
