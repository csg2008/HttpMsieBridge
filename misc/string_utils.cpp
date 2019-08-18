// Copyright (c) 2012-2014 The PHP Desktop authors. All rights reserved.
// License: New BSD License.
// Website: http://code.google.com/p/phpdesktop/

#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <functional> 
#include <cctype>
#include <random>
#include <stdio.h>
#include <wchar.h>
#include <atlstr.h>

int GetRand(int start, int end) {
    std::random_device rd;
    std::uniform_int_distribution<int> dist(start, end);

    return dist(rd);
}
int stricmp(const std::string &l,const std::string &r) {
    size_t count=l.size()<r.size()?l.size():r.size();
    for (size_t i=0;i<count;i++) {
        char lv=std::tolower(l[i]);
        char rv=std::tolower(r[i]);
        if (lv!=rv)
            return lv-rv;
    }
    return (int) (l.size()-r.size());
}
bool strieq(const std::string &l,const std::string &r) {
    return 0==stricmp(l,r);
}

std::pair<std::string,std::string> split(const std::string &s,char delim) {
    std::pair<std::string,std::string> out;
    size_t dp=s.find(delim);
    // just take the find up until the match (or to the end if no match)
    out.first.append(s,0,dp);
    // create an adjusted start position for the second string based on find status and position
    size_t sp=(dp==std::string::npos)?s.size():dp+1;
    out.second.append(s,sp,s.size()-sp);
    return out;
}
void ltrim(std::string &v) {
    size_t ec=0;
    while(ec<v.size() && std::isspace( v[ec] ))
        ec++;
    if (ec)
        v.erase(0,ec);
}
void rtrim(std::string &v) {
    size_t ec=0;
    while(ec<v.size() && std::isspace( v[v.size()-ec-1] ))
        ec++;
    if (ec)
        v.erase(v.size()-ec,ec);
}
void trim(std::string &v) {
    rtrim(v);
    ltrim(v);
}

char* Trim(char *src) {
    char *ori_src = src;

    char *begin = src;
    char *end   = src + strlen(src);

    if ( begin == end ) return ori_src; 

    while ( isblank(*begin) )          
        ++begin;
    
    while ( (*end) == '\0' || *end == ' ' || *end == '\t' || *end == '\r' || *end == '\n') 
        --end;

    if ( begin > end ) {
        *src = '\0';  return  ori_src;
    } 
    

    while ( begin != end ) {
        *src++ = *begin++;
    }

    *src++ = *end;
    *src = '\0';

    return ori_src;
}

std::string TCHAR2STRING(TCHAR *STR) {
    int iLen = WideCharToMultiByte(CP_ACP, 0,STR, -1, NULL, 0, NULL, NULL);
    char* chRtn =new char[iLen*sizeof(char)];
    WideCharToMultiByte(CP_ACP, 0, STR, -1, chRtn, iLen, NULL, NULL);
    std::string str(chRtn);
    return str;
}

wchar_t* ConvertW(const char* in) {
    CString str = CString(in); 
    USES_CONVERSION; 
    wchar_t* wszStr = new wchar_t[str.GetLength()+1]; 
    wcscpy((LPTSTR)wszStr,T2W((LPTSTR)str.GetBuffer(NULL))); 
    str.ReleaseBuffer();

    return wszStr;
}

wchar_t* char_to_wchar(const char *s) {
  DWORD size = MultiByteToWideChar(CP_UTF8, 0, s, -1, 0, 0);
  wchar_t *ws = (wchar_t *)GlobalAlloc(GMEM_FIXED, sizeof(wchar_t) * size);
  if (ws == NULL) {
    return NULL;
  }
  MultiByteToWideChar(CP_UTF8, 0, s, -1, ws, size);
  return ws;
}

char* wchar_to_char(wchar_t *ws) {
  int n = WideCharToMultiByte(CP_UTF8, 0, ws, -1, NULL, 0, NULL, NULL);
  char *s = (char *)GlobalAlloc(GMEM_FIXED, n);
  if (s == NULL) {
    return NULL;
  }
  WideCharToMultiByte(CP_UTF8, 0, ws, -1, s, n, NULL, NULL);
  return s;
}

void Utf8ToWide(const char* utf8String, wchar_t* wideString, int wideSize) {
    int copiedCharacters = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideString, wideSize);
}
std::wstring Utf8ToWide(const char* utf8String) {
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, 0, 0);
    wchar_t* wideString = new wchar_t[requiredSize];
    int copiedCharacters = MultiByteToWideChar(CP_UTF8, 0, utf8String, -1, wideString, requiredSize);
    std::wstring returnedString(wideString);
    delete[] wideString;
    wideString = 0;
    return returnedString;
}
std::wstring Utf8ToWide(const std::string& utf8String) {
    int requiredSize = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, 0, 0);
    wchar_t* wideString = new wchar_t[requiredSize];
    int copiedCharacters = MultiByteToWideChar(CP_UTF8, 0, utf8String.c_str(), -1, wideString, requiredSize);
    std::wstring returnedString(wideString);
    delete[] wideString;
    wideString = 0;
    return returnedString;
}
void WideToUtf8(const wchar_t* wideString, char* utf8String, int utf8Size) {
    int copiedBytes = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, utf8String, utf8Size, NULL, NULL);
    if (copiedBytes == 0)
        utf8String[0] = 0;
}
std::string WideToUtf8(const wchar_t* wideString) {
    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, 0, 0, NULL, NULL);
    char* utf8String = new char[requiredSize];
    int copiedBytes = WideCharToMultiByte(CP_UTF8, 0, wideString, -1, utf8String, requiredSize, NULL, NULL);
    std::string returnedString(utf8String);
    delete[] utf8String;
    utf8String = 0;
    return returnedString;
}
std::string LowerString(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    return str;
}
std::string UpperString(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), ::toupper);
    return str;
}
std::string WideToUtf8(const std::wstring& wideString) {
    int requiredSize = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, 0, 0, NULL, NULL);
    char* utf8String = new char[requiredSize];
    int copiedBytes = WideCharToMultiByte(CP_UTF8, 0, wideString.c_str(), -1, utf8String, requiredSize, NULL, NULL);
    std::string returnedString(utf8String);
    delete[] utf8String;
    utf8String = 0;
    return returnedString;
}
std::string IntToString(long number) {
    std::stringstream ss;
    ss << number;
    return ss.str();
}
std::string BoolToString(bool value) {
    if (value) {
        return "true";
    } else {
        return "false";
    }
}
std::string ReplaceString(std::string subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
    return subject;
}
void ReplaceStringInPlace(std::string& subject, const std::string& search, const std::string& replace) {
    size_t pos = 0;
    while((pos = subject.find(search, pos)) != std::string::npos) {
         subject.replace(pos, search.length(), replace);
         pos += replace.length();
    }
}
std::string &lTrimString(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    return s;
}
std::string &rTrimString(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
    return s;
}
std::string TrimString(std::string s) {
    return lTrimString(rTrimString(s));
}
std::vector<std::string> Split(const std::string &s, const std::string &seperator) {
    std::vector<std::string> result;
    std::string::size_type i = 0;
     
    while(i != s.size()){
        int flag = 0;
        while(i != s.size() && flag == 0){
            flag = 1;
            for(std::string::size_type x = 0; x < seperator.size(); ++x) {
                if(s[i] == seperator[x]) {
                    ++i;
                    flag = 0;
                    break;
                }
            }
        }

        flag = 0;
        std::string::size_type j = i;
        while(j != s.size() && flag == 0){
            for(std::string::size_type x = 0; x < seperator.size(); ++x) {
                if(s[j] == seperator[x]){
                    flag = 1;
                    break;
                }
            }

            if(flag == 0) {
                ++j;
            }
        }

        if(i != j){
            result.push_back(s.substr(i, j-i));
            i = j;
        }
    }

    return result;
}
std::vector<std::wstring> Split(const std::wstring &s, const std::wstring &seperator) {
    std::vector<std::wstring> ret;
    std::vector<std::string> source = Split(WideToUtf8(s), WideToUtf8(seperator));
    std::vector<std::string>::size_type len = source.size();

    for(std::vector<std::string>::size_type i = 0; i < len; i++) {
        ret.push_back(Utf8ToWide(source[i]));
    }

    return ret;
}

std::wstring string_to_wstring(const std::string& str)
{
	int nLen = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, NULL);
	LPWSTR lpwszStr = new wchar_t[nLen];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, lpwszStr, nLen);
	std::wstring wszStr = lpwszStr;
	delete[] lpwszStr;
	return wszStr;
}
std::string wstring_to_string(const std::wstring& wStr)
{
	int nLen = WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, NULL, 0, NULL, NULL);
	LPSTR lpszStr = new char[nLen];
	WideCharToMultiByte(CP_ACP, 0, wStr.c_str(), -1, lpszStr, nLen, NULL, NULL);
	std::string szStr = lpszStr;
	delete[] lpszStr;
	return szStr;
}


   /**
       * Replaces occurrences of string with another string.
       * Example:  string "Hello "World". from: World, to: "El Mundo"
       *        replace(str, from, to) --> string eq. "Hello El Mundo";
       *
       * WARNING: This function is not necessarily efficient.
       * @param str to change
       * @param from substring to match
       * @param to replace the matching substring with
       * @return number of replacements made
       *  */
   size_t replace(std::string& str, const std::string& from, const std::string& to) {
      if (from.empty() || str.empty()) {
         return 0;
      }

      size_t pos = 0;
      size_t count = 0;
      while ((pos = str.find(from, pos)) != std::string::npos) {
         str.replace(pos, from.length(), to);
         pos += to.length();
         ++count;
      }

      return count;
   }


   /** Trims a string's end and beginning from specified characters
    *        such as tab, space and newline i.e. "\t \n" (tab space newline)
    * @param str to clean
    * @param whitespace by default space and tab character
    * @return the cleaned string */
   std::string trim(const std::string& str, const std::string& whitespace) {
      const auto& begin = str.find_first_not_of(whitespace);
      if (std::string::npos == begin) {
         return {};
      }

      const auto& end = str.find_last_not_of(whitespace);
      const auto& range = end - begin + 1;
      return str.substr(begin, range);
   }

   /**
    * Splits a string into tokens. Tokens are split after every matched character in the delimeters string
    * @param delimiters : any matching character with the string will create a new token
    * @param stringToSplit
    * @return vector of tokens
    */
   std::vector<std::string> split(const std::string& delimiters, const std::string& stringToSplit) {
      using namespace std;
      // Skip all the text until the first delimiter is found
      string::size_type start = stringToSplit.find_first_not_of(delimiters, 0);
      string::size_type stop = stringToSplit.find_first_of(delimiters, start);

      std::vector<std::string> tokens;
      while (string::npos != stop || string::npos != start) {
         tokens.push_back(stringToSplit.substr(start, stop - start));
         start = stringToSplit.find_first_not_of(delimiters, stop);
         stop = stringToSplit.find_first_of(delimiters, start);
      }
      return tokens;
   }

   std::string to_lower(const std::string& str) {
      std::string copy(str);
      std::transform(copy.begin(), copy.end(), copy.begin(), ::tolower);
      return copy;
   }

   std::string to_upper(const std::string& str) {
      std::string copy(str);
      std::transform(copy.begin(), copy.end(), copy.begin(), ::toupper);
      return copy;
   }
   
   std::string to_string(const bool& b) {
      std::stringstream ss;
      ss << std::boolalpha << b;
      return ss.str();
   }
   
   bool to_bool(const std::string& str) {
      std::string boolStr = str;
      if (str == "1") {
         boolStr = "true";
      } else if (str == "0") {
         boolStr = "false";
      }
      std::string lowerStr = to_lower(boolStr); 
      if (lowerStr != "true" && lowerStr != "false") {
         // Currently using DeathKnell in the tests does not work as expected probably due to lack
         // of g3 integration with this namespace. Since we can't catch abort in the tests, we are 
         // returning false instead.
         return false;
      }

      std::stringstream ss(lowerStr);
      bool enabled;
      ss >> std::boolalpha >> enabled;
      return enabled;
   }

   /**
    * Explodes a string into sub-strings. Each substring is extracted after a complete
    * match of the "delimeter string"
    *
    * Explode work a litte different from split in that it is very similar to explode in php (and other languages)
    * But it uses a complete string match instead of single character (like split)
    *
    *
    * Scenario:  No match made. String returned as is, without exploding
    * Scenario:  Complete match made. Both strings equal. Complete explode. vector with 1 empty string return
    * Scenario:  string {a::c}, explode on {:}. Return { {"a"},{""},{"c"}}. Please notice the empty string
    *
    * @param complete_match : string matching where to split the content and remove the match
    * @param stringToExplode: to split at the matches
    * @return vector of exploded sub-strings
    */
   std::vector<std::string> explode(const std::string& matchAll, const std::string& toExplode) {
      if (matchAll.empty()) {
         return {toExplode};
      }

      std::vector<std::string> foundMatching;
      std::string::size_type position = 0;
      std::string::size_type keySize = matchAll.size();
      std::string::size_type found = 0;

      while ((found != std::string::npos)) {
         found = toExplode.find(matchAll, position);
         std::string item  = toExplode.substr(position, found - position);
         position = found + keySize;

         bool ignore = item.empty() && (std::string::npos == found);
         if (false == ignore) {
            foundMatching.push_back(item);
         }
      }
      return foundMatching;
   }

   /**
   * Extract blocks separated with @param complete_match_start and @param complete_match_end from the @param content
   * @return vector of blocks, including the start and end match keys
   */
   std::vector<std::string> extract(const std::string& complete_match_start, const std::string& complete_match_end,
                                    const std::string& content) {

      if (complete_match_start.size() == 0 || complete_match_end.size() == 0) {
         return {};
      }

      std::vector<std::string> result;
      const std::string::size_type startKeySize = complete_match_start.size();
      const std::string::size_type stopKeySize = complete_match_end.size();
      std::string::size_type found = 0;

      auto ContinueSearch = [](std::string::size_type found) { return found != std::string::npos;};

      while (ContinueSearch(found)) {
         auto found_start = content.find(complete_match_start, found);
         if (!ContinueSearch(found_start)) {
            break;
         }

         auto found_stop = content.find(complete_match_end, found_start + startKeySize);
         if (!ContinueSearch(found_stop)) {
            break;
         }

         found = found_stop + stopKeySize;
         auto size = found - found_start;
         result.push_back(content.substr(found_start, size));
         if (found >= content.size()) {
            break;
         }
      }

      return result;
   }
  
   bool ContainsElement(const std::vector<std::string>& v, const std::string& s) {
     return (std::find(v.begin(), v.end(), s) != v.end()); 
   }

   std::string remove_extension(const std::string& s) {
      return s.substr(0, s.find_last_of(".")); 
   }
   
   std::string get_extension(const std::string& s) {
      auto lastIndexOfPeriod = s.find_last_of(".");
      if (lastIndexOfPeriod != std::string::npos) {
         return s.substr(lastIndexOfPeriod+1, s.size()+1); 
      }
      return "";
   }