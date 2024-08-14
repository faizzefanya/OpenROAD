///////////////////////////////////////////////////////////////////////////////
// BSD 3-Clause License
//
// Copyright (c) 2019, Nefelus Inc
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.

#pragma once

#include <endian.h>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <vector>

namespace odb {

enum RecordType
{
  HEADER = 0,
  BGNLIB = 1,
  LIBNAME = 2,
  UNITS = 3,
  ENDLIB = 4,
  BGNSTR = 5,
  STRNAME = 6,
  ENDSTR = 7,
  BOUNDARY = 8,
  PATH = 9,
  SREF = 10,
  AREF = 11,
  TEXT = 12,
  LAYER = 13,
  DATATYPE = 14,
  WIDTH = 15,
  XY = 16,
  ENDEL = 17,
  SNAME = 18,
  COLROW = 19,
  TEXTNODE = 20,
  NODE = 21,
  TEXTTYPE = 22,
  PRESENTATION = 23,
  SPACING = 24,
  STRING = 25,
  STRANS = 26,
  MAG = 27,
  ANGLE = 28,
  UINTEGER = 29,
  USTRING = 30,
  REFLIBS = 31,
  FONTS = 32,
  PATHTYPE = 33,
  GENERATIONS = 34,
  ATTRTABLE = 35,
  STYPTABLE = 36,
  STRTYPE = 37,
  ELFLAGS = 38,
  ELKEY = 39,
  LINKTYPE = 40,
  LINKKEYS = 41,
  NODETYPE = 42,
  PROPATTR = 43,
  PROPVALUE = 44,
  BOX = 45,
  BOXTYPE = 46,
  PLEX = 47,
  BGNEXTN = 48,
  ENDEXTN = 49,
  TAPENUM = 50,
  TAPECODE = 51,
  STRCLASS = 52,
  RESERVED = 53,
  FORMAT = 54,
  MASK = 55,
  ENDMASKS = 56,
  LIBDIRSIZE = 57,
  SRFNAME = 58,
  LIBSECUR = 59,
  INVALID_RT = 60
};

RecordType toRecordType(uint8_t recordType);

std::string recordTypeToString(RecordType recordType);

uint8_t fromRecordType(RecordType recordType);

extern const char* recordNames[];

enum DataType
{
  NO_DATA = 0,
  BIT_ARRAY,
  INT_2,
  INT_4,
  REAL_4,
  REAL_8,
  ASCII_STRING,
  INVALID_DT
};

static const size_t dataTypeSize[DataType::INVALID_DT] = {1, 1, 2, 4, 4, 8, 1};

double real8_to_double(uint64_t real);

uint64_t double_to_real8(double value);

DataType toDataType(uint8_t dataType);

uint8_t fromDataType(DataType dataType);

struct record_t
{
  RecordType type;
  DataType dataType;
  uint16_t length;
  std::string data8;
  std::vector<int16_t> data16;
  std::vector<int32_t> data32;
  std::vector<double> data64;
};

std::map<std::pair<int16_t, int16_t>, std::string> getLayerMap(
    const std::string& filename);

}  // namespace odb