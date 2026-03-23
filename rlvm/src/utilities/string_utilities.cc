// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
//
// -----------------------------------------------------------------------
//
// This file is part of RLVM, a RealLive virtual machine clone.
//
// -----------------------------------------------------------------------
//
// Copyright (C) 2008 Elliot Glaysher
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
//
// -----------------------------------------------------------------------

#include "utilities/string_utilities.h"

#include <string>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "RLVM"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)
#endif

#include "encodings/codepage.h"
#include "utf8cpp/utf8.h"
#include "utilities/exception.h"

using std::string;
using std::wstring;

wstring cp932toUnicode(const string& line, int transformation) {
  return Cp::instance(transformation).ConvertString(line);
}

string TransformationName(int transformation) {
  switch (transformation) {
    case 0:
      return "Japanese (Cp932)";
    case 1:
      return "Chinese (Cp936)";
    case 2:
      return "Western";
    case 3:
      return "Korean (Cp949)";
    default:
      return "Unknown";
  }
}

string UnicodeToUTF8(const std::wstring& widestring) {
  string out;

  LOGI("UnicodeToUTF8: widestring.size()=%zu", widestring.size());
  if (widestring.size() > 0) {
    LOGI("UnicodeToUTF8: first wchar_t=0x%lx", (unsigned long)widestring[0]);
  }

  // Use utf32to8 since wchar_t is 32-bit on Linux/Android
  utf8::utf32to8(widestring.begin(), widestring.end(), back_inserter(out));

  LOGI("UnicodeToUTF8: out.size()=%zu", out.size());

  return out;
}

string cp932toUTF8(const string& line, int transformation) {
  if (line.empty())
    return line;

  std::wstring ws = cp932toUnicode(line, transformation);
  return UnicodeToUTF8(ws);
}

bool IsOpeningQuoteMark(int codepoint) {
  return codepoint == 0x300C || codepoint == 0x300E || codepoint == 0xFF08;
}

bool IsWrappingRomanCharacter(int codepoint) {
  if ((codepoint >= 'A' && codepoint <= 'Z') ||
      (codepoint >= 'a' && codepoint <= 'z') || codepoint == '\'' ||
      codepoint == '-') {
    return true;
  }

  // Latin Extended: accented letters used in Spanish and other western
  // languages (á, é, í, ó, ú, ñ, ü, Á, É, Í, Ó, Ú, Ñ, etc.)
  // Include these so MustLineBreak's lookahead correctly accounts for words
  // that start with an accented character.
  if ((codepoint >= 0x00C0 && codepoint <= 0x00FF) ||
      (codepoint >= 0x0100 && codepoint <= 0x02FF)) {
    return true;
  }

  return false;
}

bool IsKinsoku(int codepoint) {
  static const int matchingCodepoints[] = {
      0x0021, 0x0022, 0x0027, 0x0029, 0x002c, 0x002e, 0x003a, 0x003b, 0x003e,
      0x003f, 0x005d, 0x007d, 0x2019, 0x201d, 0x2025, 0x2026, 0x3001, 0x3002,
      0x3009, 0x300b, 0x300d, 0x300f, 0x3011, 0x301f, 0x3041, 0x3043, 0x3045,
      0x3047, 0x3049, 0x3063, 0x3083, 0x3085, 0x3087, 0x308e, 0x30a1, 0x30a3,
      0x30a5, 0x30a7, 0x30a9, 0x30c3, 0x30e3, 0x30e5, 0x30e7, 0x30ee, 0x30f5,
      0x30f6, 0x30fb, 0x30fc, 0xff01, 0xff09, 0xff0c, 0xff0e, 0xff1a, 0xff1b,
      0xff1f, 0xff3d, 0xff5d, 0xff5e, 0xff61, 0xff63, 0xff64, 0xff65, 0xff67,
      0xff68, 0xff69, 0xff6a, 0xff6b, 0xff6c, 0xff6d, 0xff6e, 0xff6f, 0xff70,
      0xff9e, 0xff9f, 0x0};

  for (int i = 0; matchingCodepoints[i] != 0x0; ++i)
    if (matchingCodepoints[i] == codepoint)
      return true;

  return false;
}

int Codepoint(const string& c) {
  if (c == "") {
    return 0;
  } else {
    string::const_iterator it = c.begin();
    return utf8::next(it, c.end());
  }
}

void AdvanceOneShiftJISChar(const char*& c) {
  if (shiftjis_lead_byte(c[0])) {
    if (c[1] == '\0') {
      throw rlvm::Exception("Malformed Shift_JIS string!");
    } else {
      c += 2;
    }
  } else {
    c += 1;
  }
}

void CopyOneShiftJisCharacter(const char*& str, string& output) {
  if (shiftjis_lead_byte(str[0])) {
    if (str[1] == '\0') {
      throw rlvm::Exception("Malformed Shift_JIS string!");
    } else {
      output += *str++;
      output += *str++;
    }
  } else {
    output += *str++;
  }
}

void CopyOneUtf8Character(const char*& str, string& output) {
  unsigned char lead = static_cast<unsigned char>(*str);

  // rldev 2-byte prefix sequences: 0x89 followed by 0x80-0xC0.
  // JisEncode() stores CP1252 chars in 0x80-0xBF range as (0x89, byte):
  //   JisEncode(ch) = ch | 0x8900  for 0x80 <= ch <= 0xBF
  //   JisEncode(0xFF) = 0x89C0
  // Decode to Unicode via Cp1252 and emit as UTF-8.
  if (lead == 0x89 && str[1] != '\0') {
    unsigned char lo = static_cast<unsigned char>(str[1]);
    if (lo >= 0x80 && lo <= 0xC0) {
      // CP1252 byte is lo (for 0x80-0xBF) or 0xFF (for 0xC0)
      unsigned char cp1252_byte = (lo == 0xC0) ? 0xFF : lo;
      // Convert CP1252 to Unicode via Cp::instance(2)
      uint16_t unicode = Cp::instance(2).Convert(cp1252_byte);
      // Encode Unicode codepoint as UTF-8
      if (unicode < 0x80) {
        output += static_cast<char>(unicode);
      } else if (unicode < 0x800) {
        output += static_cast<char>(0xC0 | (unicode >> 6));
        output += static_cast<char>(0x80 | (unicode & 0x3F));
      } else {
        output += static_cast<char>(0xE0 | (unicode >> 12));
        output += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (unicode & 0x3F));
      }
      str += 2;
      return;
    }
  }

  // Continuation byte (10xxxxxx): copy as a single raw byte.
  // This can happen when the bytecode sends bytes one at a time.
  if ((lead & 0xC0) == 0x80) {
    output += *str++;
    return;
  }

  int bytes = utf8_char_length(lead);

  // For multi-byte sequences, validate that the required continuation bytes
  // are actually present and correct (10xxxxxx pattern).
  // If they are not — the lead byte is a JisEncoded CP1252 single-byte char
  // (0xA1-0xDF, from JisEncode(cp1252_byte - 0x1F)) followed by an unrelated
  // ASCII byte.  Decode it to its real CP1252 value and emit proper UTF-8 so
  // that cp932_text_buffer holds real UTF-8.  This ensures DecodeUtf8() in
  // TextoutGetChar returns the correct codepoint for xmod width calculations.
  bool continuation_valid = true;
  for (int i = 1; i < bytes; i++) {
    if (str[i] == '\0' || (static_cast<unsigned char>(str[i]) & 0xC0) != 0x80) {
      continuation_valid = false;
      break;
    }
  }

  if (!continuation_valid) {
    if (lead >= 0xA1 && lead <= 0xDF) {
      // JisEncoded CP1252 single byte: reverse JisEncode and emit real UTF-8.
      unsigned char cp1252_byte = lead + 0x1F;
      uint16_t unicode = Cp::instance(2).Convert(cp1252_byte);
      if (unicode < 0x80) {
        output += static_cast<char>(unicode);
      } else if (unicode < 0x800) {
        output += static_cast<char>(0xC0 | (unicode >> 6));
        output += static_cast<char>(0x80 | (unicode & 0x3F));
      } else {
        output += static_cast<char>(0xE0 | (unicode >> 12));
        output += static_cast<char>(0x80 | ((unicode >> 6) & 0x3F));
        output += static_cast<char>(0x80 | (unicode & 0x3F));
      }
      str++;
    } else {
      // Other invalid lead: copy as single raw byte
      output += *str++;
    }
    return;
  }

  // Valid sequence: copy all bytes
  for (int i = 0; i < bytes && *str != '\0'; i++) {
    output += *str++;
  }
}

bool ReadFullwidthLatinLetter(const char*& str, string& output) {
  // The fullwidth uppercase latin characters are 0x8260 through 0x8279.
  if (str[0] == 0x82) {
    if (str[1] == 0) {
      throw rlvm::Exception("Malformed Shift_JIS string!");
    } else if (str[1] >= 0x60 && str[1] <= 0x79) {
      char printable = str[1] - 0x1F;
      output += printable;
      str += 2;
      return true;
    }
  }

  return false;
}

void AddShiftJISChar(uint16_t c, string& output) {
  if (c > 0xFF)
    output += (c >> 8);
  output += (c & 0xFF);
}

void PrintTextToFunction(
    std::function<bool(const string& c, const string& nextChar)> fun,
    const string& charsToPrint,
    const std::string& nextCharForFinal) {
  // Iterate over each incoming character to display (we do this
  // instead of rendering the entire string so that we can perform
  // indentation, et cetera.)
  string::const_iterator cur = charsToPrint.begin();
  string::const_iterator tmp = cur;
  string::const_iterator end = charsToPrint.end();
  utf8::next(tmp, end);
  string curChar(cur, tmp);
  for (cur = tmp; tmp != end; cur = tmp) {
    // TODO(erg): Do we have to check the return value here?
    fun(curChar, string(cur, end));

    utf8::next(tmp, end);
    curChar = string(cur, tmp);
  }

  fun(curChar, nextCharForFinal);
}

string RemoveQuotes(const string& quotedString) {
  string output = quotedString;
  if (output.size() && output[0] == '\"')
    output = output.substr(1);
  if (output.size() && output[output.size() - 1] == '\"')
    output = output.substr(0, output.size() - 2);

  return output;
}
