#include "code_convert.h"

#include <stdio.h>
#include "misc.h"

namespace CodeConvert {

/*

文字コード表
http://ash.jp/ash/src/codetbl/index.htm

*/

uint16_t sjis_to_unicode_table[0xffff];
uint16_t unicode_to_sjis_table[0xffff];

uint16_t jis_to_unicode_table[0xffff];
uint16_t unicode_to_jis_table[0xffff];

uint16_t readHexShort(char* s)
{
	return ((uint16_t)readHexByte(s[0], s[1]) << 8) + readHexByte(s[2], s[3]);
}

bool Init()
{
	char buff[256];
	{
		FILE* f = fopen("jis0201.txt", "rb");
		if (!f) {
			printf("failed to open jis0201.txt\r\n");
			return false;
		}
		while (fgets(buff, sizeof(buff), f)) {
			if (buff[0] == '#') {
				continue;
			}
			uint8_t sjis = readHexByte(buff[2], buff[3]);
			uint16_t unicode = readHexShort(&buff[7]);
			sjis_to_unicode_table[sjis] = unicode;
			unicode_to_sjis_table[unicode] = sjis;
		}
		fclose(f);
	}
	{
		FILE* f = fopen("jis0208.txt", "rb");
		if (!f) {
			printf("failed to open jis0201.txt\r\n");
			return false;
		}
		while (fgets(buff, sizeof(buff), f)) {
			if (buff[0] == '#') {
				continue;
			}
			uint16_t sjis = readHexShort(&buff[2]);
			uint16_t jis = readHexShort(&buff[9]);
			uint16_t unicode = readHexShort(&buff[16]);

			sjis_to_unicode_table[sjis] = unicode;
			unicode_to_sjis_table[unicode] = sjis;

			jis_to_unicode_table[sjis] = unicode;
			unicode_to_jis_table[unicode] = jis;
		}
		fclose(f);
	}
	return true;
}

uint16_t unicode_to_jis(uint16_t unicode)
{
	return unicode_to_jis_table[unicode];
}

} // namespace CodeConvert

