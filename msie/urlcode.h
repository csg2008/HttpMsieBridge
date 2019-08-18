#ifndef _URL_CODE_H__
#define _URL_CODE_H__

/*
 * Original C-code can be found here: http://www.geekhideout.com/urlcode.shtml
 */

#include <string>

class UrlCode {
private:
	// Converts a hex character to its integer value
	static char FromHex(char ch) {
		return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
	};

	// Converts an integer value to its hex character
	static char ToHex(char code) {
		static char hex[] = "0123456789abcdef";
		return hex[code & 15];
	}; 
public:
	static std::string Decode(std::string url) {
		const char *pstr = url.c_str();
		char *buf = new char[url.length() + 1];
		char *pbuf = buf;

		while (*pstr) {
			if (*pstr == '%') {
				if (pstr[1] && pstr[2]) {
					*pbuf++ = FromHex(pstr[1]) << 4 | FromHex(pstr[2]);
					pstr += 2;
				}
			} else if (*pstr == '+') { 
				*pbuf++ = ' ';
			} else {
				*pbuf++ = *pstr;
			}
			pstr++;
		}

		*pbuf = '\0';

		std::string decoded = buf;
		delete buf;

		return decoded;
	};
	static std::string Encode(std::string url) {
		const char *pstr = url.c_str();
		char *buf = new char[url.length() * 3 + 1];
		char *pbuf = buf;

		while (*pstr) {
			if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || *pstr == '.' || *pstr == '~') {
				*pbuf++ = *pstr;
			} else if (*pstr == ' ') {
				*pbuf++ = '+';
			} else {
				*pbuf++ = '%';
				*pbuf++ = ToHex(*pstr >> 4);
				*pbuf++ = ToHex(*pstr & 15);
			}

			pstr++;
		}

		*pbuf = '\0';

		std::string encoded = buf;
		delete buf;

		return encoded;
	};
};
#endif