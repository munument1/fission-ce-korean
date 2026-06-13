#ifndef KOREAN_FONT_H
#define KOREAN_FONT_H

namespace fallout {

bool koreanFontsEnabled();
int koreanFontGetByteCount(const char* string);
void koreanFontSetCurrent(int font);
void koreanFontsExit();

} // namespace fallout

#endif /* KOREAN_FONT_H */
