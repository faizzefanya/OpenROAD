/* Authors: Osama */
/*
 * Copyright (c) 2021, The Regents of the University of California
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the University nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dst/Distributed.h"

#include <boost/asio.hpp>
#include <boost/system/system_error.hpp>
#include <vector>

#include "LoadBalancer.h"
#include "Worker.h"
#include "dst/JobCallBack.h"
#include "dst/JobMessage.h"
#include "sta/StaMain.hh"
#include "utl/Logger.h"
#define MAX_TRIALS 5

using namespace boost::asio;
using ip::tcp;
using namespace dst;

namespace sta {
// Tcl files encoded into strings.
extern const char* dst_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Dst_Init(Tcl_Interp* interp);
}

Distributed::Distributed() : logger_(nullptr)
{
}

Distributed::~Distributed()
{
}

void Distributed::init(Tcl_Interp* tcl_interp, utl::Logger* logger)
{
  logger_ = logger;
  // Define swig TCL commands.
  Dst_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::dst_tcl_inits);
}

void Distributed::runWorker(unsigned short port)
{
  try {
    io_service io_service;
    Worker worker(io_service, this, logger_, port);
    io_service.run();
  } catch (std::exception& e) {
    logger_->error(utl::DST, 1, "Worker server error: {}", e.what());
  }
}

void Distributed::runLoadBalancer(unsigned short port)
{
  try {
    io_service io_service;
    LoadBalancer balancer(io_service, logger_, port);
    for (auto worker : workers_)
      balancer.addWorker(worker.first, worker.second, 10);
    io_service.run();
  } catch (std::exception& e) {
    logger_->error(utl::DST, 9, "LoadBalancer error: {}", e.what());
  }
}

void Distributed::addWorkerAddress(const char* address, unsigned short port)
{
  workers_.push_back({std::string(address), port});
}

bool sendMsg(tcp::socket& sock, const std::string& msg, std::string& errorMsg)
{
  int trials = 0;
  while (trials < MAX_TRIALS) {
    boost::system::error_code error;
    write(sock, buffer(msg), error);
    if (!error) {
      errorMsg = "";
      return true;
    } else
      errorMsg = error.message();
  }
  return false;
}

bool readMsg(tcp::socket& sock, std::string& dataStr)
{
  boost::system::error_code error;
  streambuf receive_buffer;
  read(sock, receive_buffer, transfer_all(), error);
  if (error && error != error::eof) {
    dataStr = error.message();
    return false;
  } else {
    const char* data = buffer_cast<const char*>(receive_buffer.data());
    dataStr = data;
    if (dataStr == "")
      return false;
    return true;
  }
}

bool Distributed::sendJob(JobMessage& msg,
                          const char* ip,
                          unsigned short port,
                          JobMessage& result)
{
  int trials = 0;
  std::string msgStr;
  if (!JobMessage::serializeMsg(JobMessage::WRITE, msg, msgStr)) {
    logger_->warn(utl::DST, 12, "Serializing JobMessage failed");
    return false;
  }
  std::string resultStr = "";
  while (trials < MAX_TRIALS) {
    trials++;
    io_service io_service;
    tcp::socket sock(io_service);
    try {
      sock.connect(tcp::endpoint(ip::address::from_string(ip), port));
    } catch (const boost::system::system_error& ex) {
      logger_->warn(utl::DST,
                    13,
                    "Socket connection failed with message \"{}\"",
                    ex.what());
      return false;
    }
    bool ok = sendMsg(sock, msgStr, resultStr);
    if (!ok)
      continue;
    ok = readMsg(sock, resultStr);
    if (!ok)
      continue;
    if (!JobMessage::serializeMsg(JobMessage::READ, result, resultStr))
      continue;
    if (sock.is_open())
      sock.close();
    return true;
  }
  if (resultStr == "")
    resultStr = "MAX_TRIALS reached";
  logger_->warn(
      utl::DST, 14, "Sending job failed with message \"{}\"", resultStr);
  return false;
}

bool Distributed::sendResult(JobMessage& msg, tcp::socket& sock)
{
  std::string msgStr;
  if (!JobMessage::serializeMsg(JobMessage::WRITE, msg, msgStr)) {
    logger_->warn(utl::DST, 20, "Serializing result JobMessage failed");
    return false;
  }
  int trials = 0;
  std::string error;
  while (trials < MAX_TRIALS) {
    if (sendMsg(sock, msgStr, error))
      return true;
  }
  logger_->warn(
      utl::DST, 22, "Sending result failed with message \"{}\"", error);
  return false;
}

void Distributed::addCallBack(JobCallBack* cb)
{
  callbacks_.push_back(cb);
}
