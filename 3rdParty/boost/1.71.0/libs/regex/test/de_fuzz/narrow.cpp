
#include <boost/regex.hpp>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *Data, size_t Size) 
{
  if(Size < 2)
    return 0;
  try{
    size_t len = (Data[1] << 8) | Data[0];
    if(len > Size - 2) len = Size - 2;
    std::string str((char*)(Data + 2), len);
    std::string text((char*)(Data + len), Size - len);
    boost::regex e(str);
    boost::smatch what;
    regex_search(text, what, e, boost::match_default|boost::match_partial);
  }
  catch(const std::exception&){}
  return 0;
}

