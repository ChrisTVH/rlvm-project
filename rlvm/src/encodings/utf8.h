/*
  RLVM: UTF-8 encoding support

  Copyright (c) 2026 RLVM Contributors

  This library is free software; you can redistribute it and/or modify it
  under the terms of the GNU Lesser General Public License as published by
  the Free Software Foundation; either version 2.1 of the License, or (at
  your option) any later version.

  This library is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License
  for more details.
*/

#ifndef SRC_ENCODINGS_UTF8_H_
#define SRC_ENCODINGS_UTF8_H_

#include <string>

#include "encodings/codepage.h"

struct Utf8 : public Codepage {
  virtual unsigned short Convert(unsigned short ch) const;
  virtual std::wstring ConvertString(const std::string& s) const;
  virtual unsigned short JisDecode(unsigned short ch) const;
  virtual void JisDecodeString(const char* s, char* buf, size_t buflen) const;
  virtual void JisEncodeString(const char* s, char* buf, size_t buflen) const;
  virtual bool DbcsDelim(char* str) const;
  Utf8();
};

// UTF-8 utility functions
unsigned int Utf8CharLength(unsigned char lead);
unsigned int DecodeUtf8(const char* s, unsigned int* out_codepoint);
std::string EncodeUtf8(unsigned int codepoint);

#endif  // SRC_ENCODINGS_UTF8_H_