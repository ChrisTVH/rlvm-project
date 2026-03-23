/*
  rlBabel: Western language settings

  Copyright (c) 2006 Peter Jolly.

  This library is free software; you can redistribute it and/or modify it under
  the terms of the GNU Lesser General Public License as published by the Free
  Software Foundation; either version 2.1 of the License, or (at your option)
  any
  later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public License
  along with this library; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

  As a special exception to the GNU Lesser General Public License (LGPL), you
  may include a publicly distributed version of the library alongside a "work
  that uses the Library" to produce a composite work that includes the library,
  and distribute that work under terms of your choice, without any of the
  additional requirements listed in clause 6 of the LGPL.

  A "publicly distributed version of the library" means either an unmodified
  binary as distributed by Haeleth, or a modified version of the library that is
  distributed under the conditions defined in clause 2 of the LGPL, and a
  "composite work that includes the library" means a RealLive program which
  links to the library, either through the LoadDLL() interface or a #DLL
  directive, and/or includes code from the library's Kepago header.

  Note that this exception does not invalidate any other reasons why any part of
  the work might be covered by the LGPL.
*/

#include "encodings/western.h"

#include <cstdint>
#include <string>

bool Cp1252::IsItalic(uint16_t ch) const {
  return (ch > 0x8300 && ch < 0x8900);
}

uint16_t GetItalic(uint16_t ch) {
  if (ch > 0x8700)
    return ch + 0x0200;
  return ch - (ch >= 0x8380 ? 0x8320 : 0x831f);
}

uint16_t Italicise(uint16_t ch) {
  if (ch == 0x20 || ch > 0xff)
    return ch;
  if (ch > 0x8900)
    return ch - 0x0200;
  return ch + (ch >= 0x60 ? 0x8320 : 0x831f);
}

Cp1252::Cp1252() {
  //  DesirableCharset = ANSI_CHARSET;
  NoTransforms = false;
  //  LANGID SysLang = GetSystemDefaultLangID();
  // For now, just bless some common European languages. (Wait, shouldn't I be
  // checking system charset instead?)
  // UseUnicode = !(SysLang & 0x1ff == 0x07 ||
  //   (SysLang & 0x1ff >= 0x09 && SysLang & 0x1ff <= 0x0c));
}

bool Cp1252::DbcsDelim(char* str) const {
  return str[0] == 0x89 &&
         (str[1] == 0x82 || str[1] == 0x84 || str[1] == 0x91 || str[1] == 0x93);
}

uint16_t Cp1252::JisDecode(uint16_t ch) const {
  if (ch <= 0x7f)
    return ch;
  if (ch >= 0xa1 && ch <= 0xdf)
    return ch + 0x1f;
  if (ch >= 0x8980 && ch <= 0x89bf)
    return ch & 0xff;
  if (ch == 0x89c0)
    return 0xff;
  if (IsItalic(ch))
    return JisDecode(GetItalic(ch));
  return 0;
}

uint16_t JisEncode(uint16_t ch) {
  if (ch <= 0x7f)
    return ch;
  if (ch >= 0x80 && ch <= 0xbf)
    return ch | 0x8900;
  if (ch >= 0xc0 && ch <= 0xfe)
    return ch - 0x1f;
  if (ch == 0xff)
    return 0x89c0;
  return 0;
}

void Cp1252::JisEncodeString(const char* src, char* buf, size_t buflen) const {
  while (*src && buflen--) {
    unsigned int ch = JisEncode(*src);
    if (ch <= 0xff) {
      *buf++ = ch;
    } else {
      *buf++ = (ch >> 8) & 0xff;
      *buf++ = ch & 0xff;
    }
    ++src;
  }
}

void Cp1252::JisDecodeString(const char* src, char* buf, size_t buflen) const {
  // CP1252 is single-byte: each byte maps directly through JisDecode
  // without Shift-JIS lead/trail byte detection.  The base class
  // implementation incorrectly treats 0x81-0x9F and 0xE0-0xEF as
  // Shift-JIS lead bytes, corrupting Latin accented characters like
  // é (0xE9), á (0xE1), etc.
  size_t srclen = std::strlen(src), i = 0, j = 0;
  while (i < srclen && j < buflen) {
    unsigned int c1 = (unsigned char)src[i++];
    unsigned int c2 = JisDecode(c1);
    if (c2 <= 0xff) {
      buf[j++] = c2;
    } else {
      buf[j++] = (c2 >> 8) & 0xff;
      buf[j++] = c2 & 0xff;
    }
  }
  buf[j] = 0;
}

// Windows-1252 specific characters (0x80-0x9F) - Unicode mapping
const uint16_t cp1252_to_uni[0x9f - 0x80 + 1] = {
    0x20ac, // 0x80 € Euro sign
    0x0081, // 0x81 (undefined, map to itself)
    0x201a, // 0x82 ‚ Single low-9 quotation mark
    0x0192, // 0x83 ƒ Latin small letter f with hook
    0x201e, // 0x84 „ Double low-9 quotation mark
    0x2026, // 0x85 … Horizontal ellipsis
    0x2020, // 0x86 † Dagger
    0x2021, // 0x87 ‡ Double dagger
    0x02c6, // 0x88 ˆ Modifier letter circumflex accent
    0x2030, // 0x89 ‰ Per mille sign
    0x0160, // 0x8A Š Latin capital letter S with caron
    0x2039, // 0x8B ‹ Single left-pointing angle quotation mark
    0x0152, // 0x8C Œ Latin capital ligature OE
    0x008d, // 0x8D (undefined, map to itself)
    0x017d, // 0x8E Ž Latin capital letter Z with caron
    0x008f, // 0x8F (undefined, map to itself)
    0x0090, // 0x90 (undefined, map to itself)
    0x2018, // 0x91 ' Left single quotation mark
    0x2019, // 0x92 ' Right single quotation mark
    0x201c, // 0x93 " Left double quotation mark
    0x201d, // 0x94 " Right double quotation mark
    0x2022, // 0x95 • Bullet
    0x2013, // 0x96 – En dash
    0x2014, // 0x97 — Em dash
    0x02dc, // 0x98 ˜ Small tilde
    0x2122, // 0x99 ™ Trade mark sign
    0x0161, // 0x9A š Latin small letter s with caron
    0x203a, // 0x9B › Single right-pointing angle quotation mark
    0x0153, // 0x9C œ Latin small ligature oe
    0x009d, // 0x9D (undefined, map to itself)
    0x017e, // 0x9E ž Latin small letter z with caron
    0x0178  // 0x9F Ÿ Latin capital letter Y with diaeresis
};

uint16_t Cp1252::Convert(uint16_t ch) const {
  return ch >= 0x80 && ch <= 0x9f ? cp1252_to_uni[ch - 0x80] : ch;
}

std::wstring Cp1252::ConvertString(const std::string& in_string) const {
  std::wstring rv;
  rv.reserve(in_string.size());

  const char* s = in_string.c_str();
  while (*s)
    rv += Convert((unsigned char)*s++);

  return rv;
}
