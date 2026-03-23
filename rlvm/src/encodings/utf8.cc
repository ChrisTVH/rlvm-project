// -*- Mode: C++; tab-width:2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
// vi:tw=80:et:ts=2:sts=2
//
// RLVM: UTF-8 encoding support
//
// Copyright (c) 2026 RLVM Contributors
//
// This library is free software; you can redistribute it and/or modify it
// under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation; either version 2.1 of the License, or (at
// your option) any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
// for more details.

#include "encodings/utf8.h"

#include <cstdint>

#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <string>

// -----------------------------------------------------------------------
// UTF-8 utility functions
// -----------------------------------------------------------------------

// Returns the number of bytes in a UTF-8 character based on the lead byte
unsigned int Utf8CharLength(unsigned char lead) {
  if (lead < 0x80) return 1;
  if ((lead & 0xE0) == 0xC0) return 2;
  if ((lead & 0xF0) == 0xE0) return 3;
  if ((lead & 0xF8) == 0xF0) return 4;
  return 1;  // Invalid, treat as single byte
}

// Decode a UTF-8 character from string, returns bytes consumed
// and stores the Unicode codepoint in *out_codepoint
unsigned int DecodeUtf8(const char* s, unsigned int* out_codepoint) {
  unsigned char c = static_cast<unsigned char>(s[0]);
  
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "RLVM", "DecodeUtf8: byte[0]=0x%02x", c);
#endif
  
  if (c < 0x80) {
    // ASCII: 0xxxxxxx
    *out_codepoint = c;
    return 1;
  }
  
  if ((c & 0xE0) == 0xC0) {
    // 2-byte: 110xxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;  // Replacement character
      return 1;
    }
    *out_codepoint = ((c & 0x1F) << 6) | (s[1] & 0x3F);
    return 2;
  }
  
  if ((c & 0xF0) == 0xE0) {
    // 3-byte: 1110xxxx 10xxxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;
      return 1;
    }
    *out_codepoint = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
    return 3;
  }
  
  if ((c & 0xF8) == 0xF0) {
    // 4-byte: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    if ((s[1] & 0xC0) != 0x80 || (s[2] & 0xC0) != 0x80 || (s[3] & 0xC0) != 0x80) {
      *out_codepoint = 0xFFFD;
      return 1;
    }
    *out_codepoint = ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | 
                     ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
    return 4;
  }
  
  // Invalid byte
  *out_codepoint = 0xFFFD;
  return 1;
}

// Encode a Unicode codepoint to UTF-8 string
std::string EncodeUtf8(unsigned int codepoint) {
  std::string result;
  
  if (codepoint < 0x80) {
    // 1 byte: 0xxxxxxx
    result += static_cast<char>(codepoint);
  } else if (codepoint < 0x800) {
    // 2 bytes: 110xxxxx 10xxxxxx
    result += static_cast<char>(0xC0 | (codepoint >> 6));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint < 0x10000) {
    // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
    result += static_cast<char>(0xE0 | (codepoint >> 12));
    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else if (codepoint < 0x110000) {
    // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    result += static_cast<char>(0xF0 | (codepoint >> 18));
    result += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    result += static_cast<char>(0x80 | (codepoint & 0x3F));
  } else {
    // Invalid codepoint, use replacement character
    result = "\xEF\xBF\xBD";
  }
  
  return result;
}

// -----------------------------------------------------------------------
// Utf8 implementation
// -----------------------------------------------------------------------

Utf8::Utf8() {
  NoTransforms = true;
  UseUnicode = 1;
}

// For UTF-8, JisDecode just passes through since UTF-8 doesn't use JIS
unsigned short Utf8::JisDecode(unsigned short ch) const {
  return ch;
}

// Convert a single UTF-8 encoded byte sequence to Unicode codepoint
unsigned short Utf8::Convert(unsigned short ch) const {
  // For single-byte values (< 0x80), return as-is (ASCII)
  if (ch < 0x80) return ch;
  // For values >= 0x80, this is not a complete UTF-8 sequence
  // The actual conversion happens in ConvertString
  return ch;
}

// Convert UTF-8 string to wide string (Unicode)
std::wstring Utf8::ConvertString(const std::string& s) const {
  std::wstring result;
  result.reserve(s.size());
  
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "RLVM", "Utf8::ConvertString: s.size()=%zu", s.size());
#endif
  
  const char* ptr = s.c_str();
  const char* end = ptr + s.size();
  
  while (ptr < end) {
    unsigned char lead = static_cast<unsigned char>(*ptr);
    
    // Skip invalid bytes (0xFF, 0xFE are never valid UTF-8)
    if (lead == 0xFF || lead == 0xFE) {
      ptr++;
      continue;
    }
    
    unsigned int codepoint;
    unsigned int bytes = DecodeUtf8(ptr, &codepoint);
    
#ifdef __ANDROID__
    __android_log_print(ANDROID_LOG_INFO, "RLVM", "Utf8::ConvertString: codepoint=0x%x, bytes=%u", codepoint, bytes);
#endif
    
    // Skip replacement characters (invalid sequences)
    if (codepoint == 0xFFFD) {
      ptr += bytes;
      continue;
    }
    
    // Convert to wchar_t (assuming wchar_t is 32-bit on Linux/Android)
    if (codepoint <= 0xFFFF) {
      result += static_cast<wchar_t>(codepoint);
    } else {
      // Handle surrogate pairs for BMP supplement (rare)
      codepoint -= 0x10000;
      result += static_cast<wchar_t>(0xD800 | (codepoint >> 10));
      result += static_cast<wchar_t>(0xDC00 | (codepoint & 0x3FF));
    }
    
    ptr += bytes;
  }
  
#ifdef __ANDROID__
  __android_log_print(ANDROID_LOG_INFO, "RLVM", "Utf8::ConvertString: result.size()=%zu", result.size());
#endif
  
  return result;
}

void Utf8::JisDecodeString(const char* src, char* buf, size_t buflen) const {
  // UTF-8 to UTF-8 (passthrough with validation)
  size_t i = 0, j = 0;
  while (src[i] && j < buflen - 1) {
    unsigned int codepoint;
    unsigned int bytes = DecodeUtf8(&src[i], &codepoint);
    
    if (codepoint == 0xFFFD) {
      // Invalid sequence, skip one byte
      i++;
    } else {
      // Valid sequence, copy it
      for (unsigned int k = 0; k < bytes && j < buflen - 1; k++) {
        buf[j++] = src[i++];
      }
    }
  }
  buf[j] = '\0';
}

void Utf8::JisEncodeString(const char* src, char* buf, size_t buflen) const {
  // UTF-8 to UTF-8 (passthrough)
  JisDecodeString(src, buf, buflen);
}

bool Utf8::DbcsDelim(char* str) const {
  // UTF-8 doesn't have DBCS delimiters in the traditional sense
  // but we can check for certain multi-byte punctuation
  unsigned int codepoint;
  unsigned int bytes = DecodeUtf8(str, &codepoint);
  
  // Check for various punctuation that might need special handling
  switch (codepoint) {
    case 0x3001:  // 、 ideographic comma
    case 0x3002:  // 。 ideographic full stop
    case 0xFF0C:  // ， fullwidth comma
    case 0xFF0E:  // ． fullwidth full stop
    case 0xFF1A:  // ： fullwidth colon
    case 0xFF1B:  // ； fullwidth semicolon
    case 0xFF1F:  // ？ fullwidth question mark
    case 0xFF01:  // ！ fullwidth exclamation mark
      return true;
    default:
      return false;
  }
}