/* Authors: Lutong Wang and Bangqi Xu */
/*
 * Copyright (c) 2019, The Regents of the University of California
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

#include "triton_route/TritonRoute.h"

#include <boost/asio/post.hpp>
#include <boost/bind/bind.hpp>
#include <fstream>
#include <iostream>

#include "DesignCallBack.h"
#include "db/tech/frTechObject.h"
#include "distributed/RoutingCallBack.h"
#include "distributed/drUpdate.h"
#include "distributed/frArchive.h"
#include "dr/FlexDR.h"
#include "dr/FlexDR_graphics.h"
#include "dst/Distributed.h"
#include "frDesign.h"
#include "frProfileTask.h"
#include "gc/FlexGC.h"
#include "global.h"
#include "gr/FlexGR.h"
#include "gui/gui.h"
#include "io/io.h"
#include "ord/OpenRoad.hh"
#include "pa/FlexPA.h"
#include "rp/FlexRP.h"
#include "serialization.h"
#include "sta/StaMain.hh"
#include "stt/SteinerTreeBuilder.h"
#include "ta/FlexTA.h"
using namespace std;
using namespace fr;
using namespace triton_route;

namespace sta {
// Tcl files encoded into strings.
extern const char* drt_tcl_inits[];
}  // namespace sta

extern "C" {
extern int Drt_Init(Tcl_Interp* interp);
}

TritonRoute::TritonRoute()
    : debug_(std::make_unique<frDebugSettings>()),
      db_callback_(std::make_unique<DesignCallBack>(this)),
      num_drvs_(-1),
      gui_(gui::Gui::get()),
      distributed_(false),
      results_sz_(0),
      dist_pool_(1)
{
}

TritonRoute::~TritonRoute()
{
}

void TritonRoute::setDebugDR(bool on)
{
  debug_->debugDR = on;
}

void TritonRoute::setDebugDumpDR(bool on, const std::string& dumpDir)
{
  debug_->debugDumpDR = on;
  debug_->dumpDir = dumpDir;
}

void TritonRoute::setDebugMaze(bool on)
{
  debug_->debugMaze = on;
}

void TritonRoute::setDebugPA(bool on)
{
  debug_->debugPA = on;
}

void TritonRoute::setDistributed(bool on)
{
  distributed_ = on;
}

void TritonRoute::setWorkerIpPort(const char* ip, unsigned short port)
{
  dist_ip_ = ip;
  dist_port_ = port;
}

void TritonRoute::setSharedVolume(const std::string& vol)
{
  shared_volume_ = vol;
  if (!shared_volume_.empty() && shared_volume_.back() != '/') {
    shared_volume_ += '/';
  }
}

void TritonRoute::setDebugNetName(const char* name)
{
  debug_->netName = name;
}

void TritonRoute::setDebugPinName(const char* name)
{
  debug_->pinName = name;
}

void TritonRoute::setDebugWorker(int x, int y)
{
  debug_->x = x;
  debug_->y = y;
}

void TritonRoute::setDebugIter(int iter)
{
  debug_->iter = iter;
}

void TritonRoute::setDebugPaMarkers(bool on)
{
  debug_->paMarkers = on;
}

void TritonRoute::setDebugPaEdge(bool on)
{
  debug_->paEdge = on;
}

void TritonRoute::setDebugPaCommit(bool on)
{
  debug_->paCommit = on;
}

void TritonRoute::setDebugWorkerParams(int mazeEndIter, int drcCost, int markerCost, int ripupMode, int followGuide)
{
  debug_->mazeEndIter = mazeEndIter;
  debug_->drcCost = drcCost;
  debug_->markerCost = markerCost;
  debug_->ripupMode = ripupMode;
  debug_->followGuide = followGuide;
}

int TritonRoute::getNumDRVs() const
{
  if (num_drvs_ < 0) {
    logger_->error(DRT, 2, "Detailed routing has not been run yet.");
  }
  return num_drvs_;
}

std::string TritonRoute::runDRWorker(const std::string& workerStr)
{
  bool on = debug_->debugDR;
  std::unique_ptr<FlexDRGraphics> graphics_
      = on && FlexDRGraphics::guiActive() ? std::make_unique<FlexDRGraphics>(
            debug_.get(), design_.get(), db_, logger_)
                                          : nullptr;
  auto worker
      = FlexDRWorker::load(workerStr, logger_, design_.get(), graphics_.get());
  worker->setSharedVolume(shared_volume_);
  worker->setDebugSettings(debug_.get());
  if (graphics_)
    graphics_->startIter(worker->getDRIter());
  std::string result = worker->reloadedMain();
  return result;
}

void TritonRoute::debugSingleWorker(const std::string& worker_path)
{
  bool on = debug_->debugDR;
  std::unique_ptr<FlexDRGraphics> graphics_
      = on && FlexDRGraphics::guiActive() ? std::make_unique<FlexDRGraphics>(
            debug_.get(), design_.get(), db_, logger_)
                                          : nullptr;
  std::ifstream workerFile(worker_path, std::ios::binary);
  std::string workerStr((std::istreambuf_iterator<char>(workerFile)), std::istreambuf_iterator<char>());
  workerFile.close();
  auto worker = FlexDRWorker::load(workerStr, logger_, design_.get(), graphics_.get());
  if(debug_->mazeEndIter != -1)
    worker->setMazeEndIter(debug_->mazeEndIter);
  if(debug_->markerCost != -1)
    worker->setMarkerCost(debug_->markerCost);
  if(debug_->drcCost != -1)
    worker->setDrcCost(debug_->drcCost);
  if(debug_->ripupMode != -1)
    worker->setRipupMode(debug_->ripupMode);
  if(debug_->followGuide != -1)
    worker->setFollowGuide((debug_->followGuide == 1));
  worker->setSharedVolume(shared_volume_);
  worker->setDebugSettings(debug_.get());
  if (graphics_)
    graphics_->startIter(worker->getDRIter());
  std::string result = worker->reloadedMain();
  bool updated = worker->end(design_.get());
  debugPrint(logger_, utl::DRT, "autotuner", 1, "End number of markers {}. Updated={}", worker->getBestNumMarkers(), updated);
}

void TritonRoute::updateGlobals(const char* file_name)
{
  std::ifstream file(file_name);
  if (!file.good())
    return;
  frIArchive ar(file);
  register_types(ar);
  serialize_globals(ar);
  file.close();
}

void TritonRoute::setGuideFile(const std::string& guide_path)
{
  GUIDE_FILE = guide_path;
}

void TritonRoute::resetDb(const char* file_name)
{
  design_ = std::make_unique<frDesign>(logger_);
  ord::OpenRoad::openRoad()->readDb(file_name);
  initDesign();
  initGuide();
  prep();
  design_->getRegionQuery()->initDRObj();
}

void TritonRoute::resetDesign(const char* file_name)
{
  std::ifstream file(file_name);
  if (!file.good())
    return;
  design_ = std::make_unique<frDesign>(logger_);
  frIArchive ar(file);
  ar.setDeepSerialize(true);
  register_types(ar);
  ar >> *(design_.get());
  file.close();
}

static void deserializeUpdate(frDesign* design,
                              const std::string& updateStr,
                              std::vector<drUpdate>& updates)
{
  std::ifstream file(updateStr.c_str());
  frIArchive ar(file);
  ar.setDeepSerialize(false);
  ar.setDesign(design);
  register_types(ar);
  ar >> updates;
  file.close();
}

static void deserializeUpdates(frDesign* design,
                               const std::string& updateStr,
                               std::vector<std::vector<drUpdate>>& updates)
{
  std::ifstream file(updateStr.c_str());
  frIArchive ar(file);
  ar.setDeepSerialize(false);
  ar.setDesign(design);
  register_types(ar);
  ar >> updates;
  file.close();
}

void TritonRoute::updateDesign(const std::vector<std::string>& updatesStrs)
{
  omp_set_num_threads(ord::OpenRoad::openRoad()->getThreadCount());
  std::vector<std::vector<drUpdate>> updates(updatesStrs.size());
#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < updatesStrs.size(); i++) {
    deserializeUpdate(design_.get(), updatesStrs.at(i), updates[i]);
  }
  applyUpdates(updates);
}

void TritonRoute::updateDesign(const std::string& path)
{
  omp_set_num_threads(ord::OpenRoad::openRoad()->getThreadCount());
  std::vector<std::vector<drUpdate>> updates;
  deserializeUpdates(design_.get(), path, updates);
  applyUpdates(updates);
}

void TritonRoute::applyUpdates(const std::vector<std::vector<drUpdate>>& updates)
{
  auto topBlock = design_->getTopBlock();
  auto regionQuery = design_->getRegionQuery();
  const auto maxSz = updates[0].size();
  for (int j = 0; j < maxSz; j++) {
    for (int i = 0; i < updates.size(); i++) {
      if (updates[i].size() <= j)
        continue;
      const auto& update = updates[i][j];
      switch (update.getType()) {
        case drUpdate::REMOVE_FROM_BLOCK: {
          auto id = update.getOrderInOwner();
          auto marker = design_->getTopBlock()->getMarker(id);
          regionQuery->removeMarker(marker);
          topBlock->removeMarker(marker);
          break;
        }
        case drUpdate::REMOVE_FROM_NET:
        case drUpdate::REMOVE_FROM_RQ: {
          auto net = update.getNet();
          auto id = update.getOrderInOwner();
          auto pinfig = net->getPinFig(id);
          switch (pinfig->typeId()) {
            case frcPathSeg: {
              auto seg = static_cast<frPathSeg*>(pinfig);
              regionQuery->removeDRObj(seg);
              if (update.getType() == drUpdate::REMOVE_FROM_NET)
                net->removeShape(seg);
              break;
            }
            case frcPatchWire: {
              auto pwire = static_cast<frPatchWire*>(pinfig);
              regionQuery->removeDRObj(pwire);
              if (update.getType() == drUpdate::REMOVE_FROM_NET)
                net->removePatchWire(pwire);
              break;
            }
            case frcVia: {
              auto via = static_cast<frVia*>(pinfig);
              regionQuery->removeDRObj(via);
              if (update.getType() == drUpdate::REMOVE_FROM_NET)
                net->removeVia(via);
              break;
            }
            default:
              logger_->error(
                  DRT, 9999, "unknown update type {}", pinfig->typeId());
              break;
          }
          break;
        }
        case drUpdate::ADD_SHAPE:
        case drUpdate::ADD_SHAPE_NET_ONLY: {
          switch (update.getObjTypeId()) {
            case frcPathSeg: {
              auto net = update.getNet();
              frPathSeg seg = update.getPathSeg();
              std::unique_ptr<frShape> uShape
                  = std::make_unique<frPathSeg>(seg);
              auto sptr = uShape.get();
              net->addShape(std::move(uShape));
              if (update.getType() == drUpdate::ADD_SHAPE)
                regionQuery->addDRObj(sptr);
              break;
            }
            case frcPatchWire: {
              auto net = update.getNet();
              frPatchWire pwire = update.getPatchWire();
              std::unique_ptr<frShape> uShape
                  = std::make_unique<frPatchWire>(pwire);
              auto sptr = uShape.get();
              net->addPatchWire(std::move(uShape));
              if (update.getType() == drUpdate::ADD_SHAPE)
                regionQuery->addDRObj(sptr);
              break;
            }
            case frcVia: {
              auto net = update.getNet();
              frVia via = update.getVia();
              auto uVia = std::make_unique<frVia>(via);
              auto sptr = uVia.get();
              net->addVia(std::move(uVia));
              if (update.getType() == drUpdate::ADD_SHAPE)
                regionQuery->addDRObj(sptr);
              break;
            }
            default: {
              frMarker marker = update.getMarker();
              auto uMarker = std::make_unique<frMarker>(marker);
              auto sptr = uMarker.get();
              topBlock->addMarker(std::move(uMarker));
              regionQuery->addMarker(sptr);
              break;
            }
          }
          break;
        }
        case drUpdate::ADD_GUIDE: {
          frPathSeg seg = update.getPathSeg();
          std::unique_ptr<frPathSeg> uSeg = std::make_unique<frPathSeg>(seg);
          auto net = update.getNet();
          uSeg->addToNet(net);
          vector<unique_ptr<frConnFig>> tmp;
          tmp.push_back(std::move(uSeg));
          auto idx = update.getOrderInOwner();
          if (idx < 0 || idx >= net->getGuides().size())
            logger_->error(DRT,
                           9199,
                           "Guide {} out of range {}",
                           idx,
                           net->getGuides().size());
          const auto& guide = net->getGuides().at(idx);
          guide->setRoutes(tmp);
          break;
        }
        case drUpdate::UPDATE_SHAPE: {
          auto net = update.getNet();
          auto id = update.getOrderInOwner();
          auto pinfig = net->getPinFig(id);
          switch (pinfig->typeId()) {
            case frcPathSeg: {
              auto seg = static_cast<frPathSeg*>(pinfig);
              frPathSeg updatedSeg = update.getPathSeg();
              seg->setPoints(updatedSeg.getBeginPoint(),
                             updatedSeg.getEndPoint());
              frSegStyle style;
              updatedSeg.getStyle(style);
              seg->setStyle(style);
              regionQuery->addDRObj(seg);
              break;
            }
            default:
              break;
          }
        }
      }
    }
  }
}

void TritonRoute::init(Tcl_Interp* tcl_interp,
                       odb::dbDatabase* db,
                       Logger* logger,
                       dst::Distributed* dist,
                       stt::SteinerTreeBuilder* stt_builder)
{
  db_ = db;
  logger_ = logger;
  dist_ = dist;
  stt_builder_ = stt_builder;
  design_ = std::make_unique<frDesign>(logger_);
  dist->addCallBack(new fr::RoutingCallBack(this, dist, logger));
  // Define swig TCL commands.
  Drt_Init(tcl_interp);
  sta::evalTclInit(tcl_interp, sta::drt_tcl_inits);
  FlexDRGraphics::init();
}

void TritonRoute::initGuide()
{
  if (DBPROCESSNODE == "GF14_13M_3Mx_2Cx_4Kx_2Hx_2Gx_LB")
    USENONPREFTRACKS = false;
  io::Parser parser(getDesign(), logger_);
  if (!GUIDE_FILE.empty()) {
    parser.readGuide();
    parser.postProcessGuide(db_);
  }
  parser.initRPin();
}
void TritonRoute::initDesign()
{
  if (getDesign()->getTopBlock() != nullptr)
    return;
  io::Parser parser(getDesign(), logger_);
  parser.readDb(db_);
  auto tech = getDesign()->getTech();
  if (!BOTTOM_ROUTING_LAYER_NAME.empty()) {
    frLayer* layer = tech->getLayer(BOTTOM_ROUTING_LAYER_NAME);
    if (layer) {
      BOTTOM_ROUTING_LAYER = layer->getLayerNum();
    } else {
      logger_->warn(utl::DRT,
                    272,
                    "bottomRoutingLayer {} not found.",
                    BOTTOM_ROUTING_LAYER_NAME);
    }
  }

  if (!TOP_ROUTING_LAYER_NAME.empty()) {
    frLayer* layer = tech->getLayer(TOP_ROUTING_LAYER_NAME);
    if (layer) {
      TOP_ROUTING_LAYER = layer->getLayerNum();
    } else {
      logger_->warn(utl::DRT,
                    273,
                    "topRoutingLayer {} not found.",
                    TOP_ROUTING_LAYER_NAME);
    }
  }

  if (!VIAINPIN_BOTTOMLAYER_NAME.empty()) {
    frLayer* layer = tech->getLayer(VIAINPIN_BOTTOMLAYER_NAME);
    if (layer) {
      VIAINPIN_BOTTOMLAYERNUM = layer->getLayerNum();
    } else {
      logger_->warn(utl::DRT,
                    606,
                    "via in pin bottom layer {} not found.",
                    VIAINPIN_BOTTOMLAYERNUM);
    }
  }

  if (!VIAINPIN_TOPLAYER_NAME.empty()) {
    frLayer* layer = tech->getLayer(VIAINPIN_TOPLAYER_NAME);
    if (layer) {
      VIAINPIN_TOPLAYERNUM = layer->getLayerNum();
    } else {
      logger_->warn(utl::DRT,
                    607,
                    "via in pin top layer {} not found.",
                    VIAINPIN_TOPLAYERNUM);
    }
  }
  parser.postProcess();
  if (db_ != nullptr && db_->getChip() != nullptr
      && db_->getChip()->getBlock() != nullptr)
    db_callback_->addOwner(db_->getChip()->getBlock());
}

void TritonRoute::prep()
{
  FlexRP rp(getDesign(), getDesign()->getTech(), logger_);
  rp.main();
}

void TritonRoute::gr()
{
  FlexGR gr(getDesign(), logger_, stt_builder_);
  gr.main(db_);
}

void TritonRoute::ta()
{
  FlexTA ta(getDesign(), logger_);
  ta.main();
}

void TritonRoute::dr()
{
  num_drvs_ = -1;
  dr_ = std::make_unique<FlexDR>(this, getDesign(), logger_, db_);
  dr_->setDebug(debug_.get());
  if (distributed_)
    dr_->setDistributed(dist_, dist_ip_, dist_port_, shared_volume_);
  if (SINGLE_STEP_DR) {
    dr_->init();
  } else {
    dr_->main();
  }
}

void TritonRoute::stepDR(int size,
                         int offset,
                         int mazeEndIter,
                         frUInt4 workerDRCCost,
                         frUInt4 workerMarkerCost,
                         int ripupMode,
                         bool followGuide)
{
  dr_->searchRepair({size,
                     offset,
                     mazeEndIter,
                     workerDRCCost,
                     workerMarkerCost,
                     ripupMode,
                     followGuide});
  num_drvs_ = design_->getTopBlock()->getNumMarkers();
}

void TritonRoute::endFR()
{
  if (SINGLE_STEP_DR) {
    dr_->end(/* done */ true);
  }
  dr_.reset();
  io::Writer writer(getDesign(), logger_);
  writer.updateDb(db_);

  num_drvs_ = design_->getTopBlock()->getNumMarkers();
}

void TritonRoute::reportConstraints()
{
  getDesign()->getTech()->printAllConstraints(logger_);
}

bool TritonRoute::writeGlobals(const std::string& name)
{
  std::ofstream file(name);
  if (!file.good())
    return false;
  frOArchive ar(file);
  register_types(ar);
  serialize_globals(ar);
  file.close();
  return true;
}
static bool serialize_design(frDesign* design, const std::string& name)
{
  ProfileTask t1("DIST: SERIALIZE_DESIGN");
  ProfileTask t1_version(std::string("DIST: SERIALIZE" + name).c_str());
  std::stringstream stream(std::ios_base::binary | std::ios_base::in
                           | std::ios_base::out);
  frOArchive ar(stream);
  ar.setDeepSerialize(true);
  register_types(ar);
  ar << *design;
  t1.done();
  t1_version.done();
  ProfileTask t2("DIST: WRITE_DESIGN");
  ProfileTask t2_version(std::string("DIST: WRITE" + name).c_str());
  std::ofstream file(name);
  if (!file.good())
    return false;
  file << stream.rdbuf();
  file.close();
  return true;
}
void TritonRoute::sendFrDesignDist()
{
  if (distributed_) {
    std::string design_path = fmt::format("{}DESIGN.db", shared_volume_);
    std::string globals_path = fmt::format("{}DESIGN.globals", shared_volume_);
    serialize_design(design_.get(), design_path);
    writeGlobals(globals_path.c_str());
    dst::JobMessage msg(dst::JobMessage::UPDATE_DESIGN,
                        dst::JobMessage::BROADCAST),
        result(dst::JobMessage::NONE);
    std::unique_ptr<dst::JobDescription> desc
        = std::make_unique<RoutingJobDescription>();
    RoutingJobDescription* rjd
        = static_cast<RoutingJobDescription*>(desc.get());
    rjd->setDesignPath(design_path);
    rjd->setSharedDir(shared_volume_);
    rjd->setGlobalsPath(globals_path);
    rjd->setDesignUpdate(false);
    msg.setJobDescription(std::move(desc));
    bool ok = dist_->sendJob(msg, dist_ip_.c_str(), dist_port_, result);
    if (!ok)
      logger_->error(DRT, 13304, "Updating design remotely failed");
  }
  design_->getRegionQuery()->dummyUpdate();
  design_->clearUpdates();
}

void TritonRoute::sendDesignDist()
{
  if (distributed_) {
    std::string design_path = fmt::format("{}DESIGN.db", shared_volume_);
    std::string guide_path = fmt::format("{}DESIGN.guide", shared_volume_);
    std::string globals_path = fmt::format("{}DESIGN.globals", shared_volume_);
    ord::OpenRoad::openRoad()->writeDb(design_path.c_str());
    std::ifstream src(GUIDE_FILE, std::ios::binary);
    std::ofstream dst(guide_path.c_str(), std::ios::binary);
    dst << src.rdbuf();
    dst.close();
    writeGlobals(globals_path.c_str());
    dst::JobMessage msg(dst::JobMessage::UPDATE_DESIGN,
                        dst::JobMessage::BROADCAST),
        result(dst::JobMessage::NONE);
    std::unique_ptr<dst::JobDescription> desc
        = std::make_unique<RoutingJobDescription>();
    RoutingJobDescription* rjd
        = static_cast<RoutingJobDescription*>(desc.get());
    rjd->setDesignPath(design_path);
    rjd->setSharedDir(shared_volume_);
    rjd->setGuidePath(guide_path);
    rjd->setGlobalsPath(globals_path);
    rjd->setDesignUpdate(false);
    msg.setJobDescription(std::move(desc));
    bool ok = dist_->sendJob(msg, dist_ip_.c_str(), dist_port_, result);
    if (!ok)
      logger_->error(DRT, 12304, "Updating design remotely failed");
  }
  design_->clearUpdates();
}
static void serializeUpdatesBatch(const std::vector<drUpdate>& batch,
                                  const std::string& file_name)
{
  std::ofstream file(file_name.c_str());
  frOArchive ar(file);
  ar.setDeepSerialize(false);
  register_types(ar);
  ar << batch;
  file.close();
}

void TritonRoute::sendGlobalsUpdates(const std::string& globals_path)
{
  if (!distributed_)
    return;
  ProfileTask task("DIST: SENDING GLOBALS");
  dst::JobMessage msg(dst::JobMessage::UPDATE_DESIGN,
                      dst::JobMessage::BROADCAST),
      result(dst::JobMessage::NONE);
  std::unique_ptr<dst::JobDescription> desc
      = std::make_unique<RoutingJobDescription>();
  RoutingJobDescription* rjd = static_cast<RoutingJobDescription*>(desc.get());
  rjd->setGlobalsPath(globals_path);
  rjd->setSharedDir(shared_volume_);
  msg.setJobDescription(std::move(desc));
  bool ok = dist_->sendJob(msg, dist_ip_.c_str(), dist_port_, result);
  if (!ok)
    logger_->error(DRT, 9504, "Updating globals remotely failed");
}

void TritonRoute::sendDesignUpdates(const std::string& globals_path)
{
  if (!distributed_)
    return;
  if (!design_->hasUpdates())
    return;
  std::unique_ptr<ProfileTask> serializeTask;
  if (design_->getVersion() == 0)
    serializeTask = std::make_unique<ProfileTask>("DIST: SERIALIZE_TA");
  else
    serializeTask = std::make_unique<ProfileTask>("DIST: SERIALIZE_UPDATES");
  const auto& designUpdates = design_->getUpdates();
  omp_set_num_threads(MAX_THREADS);
  std::vector<std::string> updates(designUpdates.size());
#pragma omp parallel for schedule(dynamic)
  for (int i = 0; i < designUpdates.size(); i++) {
    updates[i] = fmt::format("{}updates_{}.bin", shared_volume_, i);
    serializeUpdatesBatch(designUpdates.at(i), updates[i]);
  }
  serializeTask->done();
  std::unique_ptr<ProfileTask> task;
  if (design_->getVersion() == 0)
    task = std::make_unique<ProfileTask>("DIST: SENDING_TA");
  else
    task = std::make_unique<ProfileTask>("DIST: SENDING_UDPATES");
  dst::JobMessage msg(dst::JobMessage::UPDATE_DESIGN,
                      dst::JobMessage::BROADCAST),
      result(dst::JobMessage::NONE);
  std::unique_ptr<dst::JobDescription> desc
      = std::make_unique<RoutingJobDescription>();
  RoutingJobDescription* rjd = static_cast<RoutingJobDescription*>(desc.get());
  rjd->setUpdates(updates);
  rjd->setGlobalsPath(globals_path);
  rjd->setSharedDir(shared_volume_);
  rjd->setDesignUpdate(true);
  msg.setJobDescription(std::move(desc));
  bool ok = dist_->sendJob(msg, dist_ip_.c_str(), dist_port_, result);
  if (!ok)
    logger_->error(DRT, 304, "Updating design remotely failed");
  task->done();
  design_->clearUpdates();
  design_->incrementVersion();
}

int TritonRoute::main()
{
  MAX_THREADS = ord::OpenRoad::openRoad()->getThreadCount();
  if (distributed_ && NO_PA) {
    asio::post(dist_pool_, boost::bind(&TritonRoute::sendDesignDist, this));
  }
  initDesign();
  if (!NO_PA) {
    FlexPA pa(getDesign(), logger_);
    pa.setDebug(debug_.get(), db_);
    pa.main();
    if (distributed_ || debug_->debugDumpDR) {
      io::Writer writer(getDesign(), logger_);
      writer.updateDb(db_, true);
      asio::post(dist_pool_, boost::bind(&TritonRoute::sendDesignDist, this));
    }
  }
  if(debug_->debugDumpDR)
  {
    ord::OpenRoad::openRoad()->writeDb(fmt::format("{}/design.db", debug_->dumpDir).c_str());
  }
  initGuide();
  if (GUIDE_FILE == string("")) {
    gr();
    io::Parser parser(getDesign(), logger_);
    GUIDE_FILE = OUTGUIDE_FILE;
    ENABLE_VIA_GEN = true;
    parser.readGuide();
    parser.initDefaultVias();
    parser.postProcessGuide(db_);
  }
  prep();
  ta();
  if (distributed_) {
    // sendDesignUpdates("");
    asio::post(dist_pool_,
               boost::bind(&TritonRoute::sendDesignUpdates, this, ""));
  }
  dr();
  if (!SINGLE_STEP_DR) {
    endFR();
  }
  return 0;
}

void TritonRoute::pinAccess(std::vector<odb::dbInst*> target_insts)
{
  MAX_THREADS = ord::OpenRoad::openRoad()->getThreadCount();
  ENABLE_VIA_GEN = true;
  initDesign();
  FlexPA pa(getDesign(), logger_);
  pa.setTargetInstances(target_insts);
  pa.setDebug(debug_.get(), db_);
  pa.main();
  io::Writer writer(getDesign(), logger_);
  writer.updateDb(db_, true);
}

void TritonRoute::readParams(const string& fileName)
{
  logger_->warn(utl::DRT, 252, "params file is deprecated. Use tcl arguments.");

  int readParamCnt = 0;
  ifstream fin(fileName.c_str());
  string line;
  if (fin.is_open()) {
    while (fin.good()) {
      getline(fin, line);
      if (line[0] != '#') {
        char delimiter = ':';
        int pos = line.find(delimiter);
        string field = line.substr(0, pos);
        string value = line.substr(pos + 1);
        stringstream ss(value);
        if (field == "lef") {
          logger_->warn(utl::DRT, 148, "Deprecated lef param in params file.");
        } else if (field == "def") {
          logger_->warn(utl::DRT, 227, "Deprecated def param in params file.");
        } else if (field == "guide") {
          GUIDE_FILE = value;
          ++readParamCnt;
        } else if (field == "outputTA") {
          logger_->warn(
              utl::DRT, 266, "Deprecated outputTA param in params file.");
        } else if (field == "output") {
          logger_->warn(
              utl::DRT, 205, "Deprecated output param in params file.");
        } else if (field == "outputguide") {
          OUTGUIDE_FILE = value;
          ++readParamCnt;
        } else if (field == "outputMaze") {
          OUT_MAZE_FILE = value;
          ++readParamCnt;
        } else if (field == "outputDRC") {
          DRC_RPT_FILE = value;
          ++readParamCnt;
        } else if (field == "outputCMap") {
          CMAP_FILE = value;
          ++readParamCnt;
        } else if (field == "threads") {
          logger_->warn(utl::DRT,
                        274,
                        "Deprecated threads param in params file."
                        " Use 'set_thread_count'.");
          ++readParamCnt;
        } else if (field == "verbose")
          VERBOSE = atoi(value.c_str());
        else if (field == "dbProcessNode") {
          DBPROCESSNODE = value;
          ++readParamCnt;
        } else if (field == "viaInPinBottomLayer") {
          VIAINPIN_BOTTOMLAYER_NAME = value;
          ++readParamCnt;
        } else if (field == "viaInPinTopLayer") {
          VIAINPIN_TOPLAYER_NAME = value;
          ++readParamCnt;
        } else if (field == "drouteEndIterNum") {
          END_ITERATION = atoi(value.c_str());
          ++readParamCnt;
        } else if (field == "OR_SEED") {
          OR_SEED = atoi(value.c_str());
          ++readParamCnt;
        } else if (field == "OR_K") {
          OR_K = atof(value.c_str());
          ++readParamCnt;
        } else if (field == "bottomRoutingLayer") {
          BOTTOM_ROUTING_LAYER_NAME = value;
          ++readParamCnt;
        } else if (field == "topRoutingLayer") {
          TOP_ROUTING_LAYER_NAME = value;
          ++readParamCnt;
        } else if (field == "initRouteShapeCost") {
          ROUTESHAPECOST = atoi(value.c_str());
          ++readParamCnt;
        } else if (field == "clean_patches")
          CLEAN_PATCHES = true;
      }
    }
    fin.close();
  }

  if (readParamCnt < 2) {
    logger_->error(DRT, 1, "Error reading param file: {}.", fileName);
  }
}

void TritonRoute::addUserSelectedVia(const std::string& viaName)
{
  if (db_->getChip() == nullptr || db_->getChip()->getBlock() == nullptr
      || db_->getTech() == nullptr) {
    logger_->error(DRT, 610, "Load design before setting default vias");
  }
  auto block = db_->getChip()->getBlock();
  auto tech = db_->getTech();
  if (tech->findVia(viaName.c_str()) == nullptr
      && block->findVia(viaName.c_str()) == nullptr) {
    logger_->error(utl::DRT, 611, "Via {} not found", viaName);
  } else {
    design_->addUserSelectedVia(viaName);
  }
}

void TritonRoute::setParams(const ParamStruct& params)
{
  GUIDE_FILE = params.guideFile;
  OUTGUIDE_FILE = params.outputGuideFile;
  OUT_MAZE_FILE = params.outputMazeFile;
  DRC_RPT_FILE = params.outputDrcFile;
  CMAP_FILE = params.outputCmapFile;
  VERBOSE = params.verbose;
  ENABLE_VIA_GEN = params.enableViaGen;
  DBPROCESSNODE = params.dbProcessNode;
  CLEAN_PATCHES = params.cleanPatches;
  NO_PA = params.noPa;
  SINGLE_STEP_DR = params.singleStepDR;
  if (!params.viaInPinBottomLayer.empty()) {
    VIAINPIN_BOTTOMLAYER_NAME = params.viaInPinBottomLayer;
  }
  if (!params.viaInPinTopLayer.empty()) {
    VIAINPIN_TOPLAYER_NAME = params.viaInPinTopLayer;
  }
  if (params.drouteEndIter >= 0) {
    END_ITERATION = params.drouteEndIter;
  }
  OR_SEED = params.orSeed;
  OR_K = params.orK;
  if (!params.bottomRoutingLayer.empty()) {
    BOTTOM_ROUTING_LAYER_NAME = params.bottomRoutingLayer;
  }
  if (!params.topRoutingLayer.empty()) {
    TOP_ROUTING_LAYER_NAME = params.topRoutingLayer;
  }
}

void TritonRoute::addWorkerResults(
    const std::vector<std::pair<int, std::string>>& results)
{
  std::unique_lock<std::mutex> lock(results_mutex_);
  workers_results_.insert(
      workers_results_.end(), results.begin(), results.end());
  results_sz_ = workers_results_.size();
}

bool TritonRoute::getWorkerResults(
    std::vector<std::pair<int, std::string>>& results)
{
  std::unique_lock<std::mutex> lock(results_mutex_);
  if (workers_results_.empty())
    return false;
  results = workers_results_;
  workers_results_.clear();
  results_sz_ = 0;
  return true;
}

int TritonRoute::getWorkerResultsSize()
{
  return results_sz_;
}
