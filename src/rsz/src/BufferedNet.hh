/////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2019, The Regents of the University of California
// All rights reserved.
//
// BSD 3-Clause License
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
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <array>
#include <memory>

#include "utl/Logger.h"
#include "spdlog/fmt/fmt.h"
#include "odb/geom.h"

#include "sta/Transition.hh"
#include "sta/Network.hh"
#include "sta/Delay.hh"
#include "sta/PathRef.hh"

namespace rsz {

using std::array;
using std::string;

using utl::Logger;

using odb::Point;

using sta::LibertyCell;
using sta::Pin;
using sta::Required;
using sta::RiseFall;
using sta::Network;
using sta::Unit;
using sta::Units;
using sta::PathRef;
using sta::Delay;
using sta::StaState;
using sta::DcalcAnalysisPt;

class BufferedNet;
class Resizer;

typedef std::shared_ptr<BufferedNet> BufferedNetPtr;
typedef array<Required, RiseFall::index_count> Requireds;

enum class BufferedNetType { load, junction, wire, buffer };

// The routing tree is represented a binary tree with the sinks being the leaves
// of the tree, the junctions being the Steiner nodes and the root being the
// source of the net.
class BufferedNet
{
public:
  // load
  BufferedNet(BufferedNetType type,
              Point location,
              Pin *load_pin);
  // junc
  BufferedNet(BufferedNetType type,
              Point location,
              BufferedNetPtr ref,
              BufferedNetPtr ref2);
  // wire
  BufferedNet(BufferedNetType type,
              Point location,
              int layer,
              BufferedNetPtr ref);
  // buffer
  BufferedNet(BufferedNetType type,
              Point location,
              LibertyCell *buffer_cell,
              BufferedNetPtr ref);
  string to_string(Resizer *resizer);
  void reportTree(Resizer *resizer);
  void reportTree(int level,
                  Resizer *resizer);
  BufferedNetType type() const { return type_; }
  // junction steiner point location connecting ref/ref2
  // wire     wire is from loc to location(ref_)
  // buffer   buffer driver pin location
  // load     load pin location
  Point location() const { return location_; }
  // buffer
  LibertyCell *bufferCell() const { return buffer_cell_; }
  // load
  Pin *loadPin() const { return load_pin_; }
  int layer() const { return layer_; }
  // junction  left
  // buffer    wire
  // wire      end of wire
  BufferedNetPtr ref() const { return ref_; }
  // junction  right
  BufferedNetPtr ref2() const { return ref2_; }

  // repairNet
  int maxLoadWireLength() const;

  // Rebuffer
  float cap() const { return cap_; }
  void setCapacitance(float cap);
  Required required(StaState *sta);
  const PathRef &requiredPath() const { return required_path_; }
  void setRequiredPath(const PathRef &path_ref);
  Delay requiredDelay() const { return required_delay_; }
  void setRequiredDelay(Delay delay);
  // Downstream buffer count.
  int bufferCount() const;

  static constexpr int null_layer = -1;

private:
  BufferedNetType type_;
  Point location_;
  // load
  Pin *load_pin_;
  // buffer
  LibertyCell *buffer_cell_;
  // wire
  int layer_;
  // load wire junc
  BufferedNetPtr ref_;
  // junc
  BufferedNetPtr ref2_;

  // Rebuffer annotations
  // Capacitance looking downstream from here.
  float cap_;
  // PathRef for worst required path at load.
  PathRef required_path_;
  // Max delay from here to the loads.
  Delay required_delay_;
};

} // namespace
