
#include <boost/regex.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) 
{
  if(Size < 2)
    return 0;
  std::vector<wchar_t> v(Data, Data + Size);
  try{
    size_t len = (Data[1] << 8) | Data[0];
    if(len > Size - 2) len = Size - 2;
    std::wstring str(&v[0] + 2, len);
    std::wstring text(&v[0] + len, Size - len);
    boost::wregex e(str);
    boost::wsmatch what;
    regex_search(text, what, e, boost::match_default|boost::match_partial);
  }
  catch(const std::exception&){}
  return 0;
}

