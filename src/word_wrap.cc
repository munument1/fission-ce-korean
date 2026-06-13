#include "word_wrap.h"

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#include "korean_font.h"
#include "text_font.h"

namespace fallout {

// 0x4BC6F0
int wordWrap(const char* string, int width, short* breakpoints, short* breakpointsLengthPtr)
{
    breakpoints[0] = 0;
    *breakpointsLengthPtr = 1;

    for (int index = 1; index < WORD_WRAP_MAX_COUNT; index++) {
        breakpoints[index] = -1;
    }

    if (fontGetMonospacedCharacterWidth() > width) {
        return -1;
    }

    if (fontGetStringWidth(string) < width) {
        breakpoints[*breakpointsLengthPtr] = (short)strlen(string);
        *breakpointsLengthPtr += 1;
        return 0;
    }

    int gap = fontGetLetterSpacing();

    int accum = 0;
    const char* prevSpaceOrHyphen = nullptr;
    const char* pch = string;
    while (*pch != '\0') {
        unsigned char c1 = *pch;
        int charLen = koreanFontGetByteCount(pch);

        int charWidth = 0;
        if (charLen == 2) {
            charWidth = fontGetCharacterWidth((c1 << 8) | ((unsigned char)*(pch + 1)));
        } else {
            charWidth = fontGetCharacterWidth(c1);
        }

        accum += gap + charWidth;
        if (accum <= width) {
            if (charLen == 1 && (isspace(c1) || c1 == '-')) {
                prevSpaceOrHyphen = pch;
            }
        } else {
            if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
                return -1;
            }

            if (prevSpaceOrHyphen != nullptr) {
                // Word wrap.
                breakpoints[*breakpointsLengthPtr] = prevSpaceOrHyphen - string + 1;
                *breakpointsLengthPtr += 1;

                pch = prevSpaceOrHyphen + 1;
                prevSpaceOrHyphen = nullptr;
                accum = 0;
                continue;
            } else {
                // Character wrap.
                breakpoints[*breakpointsLengthPtr] = pch - string;
                *breakpointsLengthPtr += 1;

                accum = 0;
                prevSpaceOrHyphen = nullptr;
                continue;
            }
        }
        pch += charLen;
    }

    if (*breakpointsLengthPtr == WORD_WRAP_MAX_COUNT) {
        return -1;
    }

    breakpoints[*breakpointsLengthPtr] = pch - string + 1;
    *breakpointsLengthPtr += 1;

    return 0;
}

} // namespace fallout
