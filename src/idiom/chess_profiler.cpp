// Copyright 2011 The University of Michigan
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Authors - Jie Yu (jieyu@umich.edu)

// File: idiom/chess_profiler.cpp - Implementation of the controller
// for idiom CHESS profiler.

#include "idiom/chess_profiler.hpp"

namespace idiom {

ChessProfiler::ChessProfiler()
    : iroot_db_(NULL),
      memo_(NULL),
      sinst_db_(NULL),
      sinst_analyzer_(NULL),
      observer_(NULL),
      observer_new_(NULL) {
  // empty
}

void ChessProfiler::HandlePreSetup() {
  systematic::Controller::HandlePreSetup();

  knob_->RegisterBool("ignore_ic_pthread", "do not count instructions in pthread", "1");
  knob_->RegisterBool("ignore_lib", "whether ignore accesses from common libraries", "0");
  knob_->RegisterBool("memo_failed", "Whether memoize failed-to-expose iroots.", "1");
  knob_->RegisterStr("iroot_in", "The input iroot database.", "iroot.db");
  knob_->RegisterStr("iroot_out", "The output iroot database", "iroot.db");
  knob_->RegisterStr("memo_in", "The input memorization database.", "memo.db");
  knob_->RegisterStr("memo_out", "The output memorization database.", "memo.db");
  knob_->RegisterStr("sinst_in", "The input shared instruction database.", "sinst.db");
  knob_->RegisterStr("sinst_out", "The output shared instruction database.", "sinst.db");

  sinst_analyzer_ = new sinst::SharedInstAnalyzer(knob_);
  observer_ = new Observer(knob_);
  observer_new_ = new ObserverNew(knob_);
  sinst_analyzer_->Register();
  observer_->Register();
  observer_new_->Register();
}

void ChessProfiler::HandlePostSetup() {
  systematic::Controller::HandlePostSetup();

  // load iroot db
  iroot_db_ = new iRootDB(CreateMutex());
  iroot_db_->Load(knob_->ValueStr("iroot_in"), sinfo_);
  // load memoization db
  memo_ = new Memo(CreateMutex(), iroot_db_);
  memo_->Load(knob_->ValueStr("memo_in"), sinfo_);
  // load shared inst db
  sinst_db_ = new sinst::SharedInstDB(CreateMutex());
  sinst_db_->Load(knob_->ValueStr("sinst_in"), sinfo_);

  if (sinst_analyzer_->Enabled()) {
    // create sinst analyzer
    sinst_analyzer_->Setup(CreateMutex(), sinst_db_);
    AddAnalyzer(sinst_analyzer_);
  }

  // make sure that we only use one type of observer
  if (observer_->Enabled() && observer_new_->Enabled())
    Abort("Please choose an observer.\n");

  if (observer_->Enabled()) {
    // create iRoot observer
    observer_->Setup(CreateMutex(), sinfo_, iroot_db_, memo_, sinst_db_);
    AddAnalyzer(observer_);
  }

  if (observer_new_->Enabled()) {
    // create iRoot observer (NEW)
    observer_new_->Setup(CreateMutex(), sinfo_, iroot_db_, memo_, sinst_db_);
    AddAnalyzer(observer_new_);
  }
}

bool ChessProfiler::HandleIgnoreInstCount(IMG img) {
  if (knob_->ValueBool("ignore_ic_pthread")) {
    if (!IMG_Valid(img))
      return false;
    Image *image = sinfo_->FindImage(IMG_Name(img));
    DEBUG_ASSERT(image);
    if (image->IsPthread())
      return true;
  }
  return false;
}

bool ChessProfiler::HandleIgnoreMemAccess(IMG img) {
  if (!IMG_Valid(img))
    return true;
  Image *image = sinfo_->FindImage(IMG_Name(img));
  DEBUG_ASSERT(image);
  if (image->IsPthread())
    return true;
  if (knob_->ValueBool("ignore_lib")) {
    if (image->IsCommonLib())
      return true;
  }
  return false;
}

void ChessProfiler::HandleProgramExit() {
  systematic::Controller::HandleProgramExit();

  memo_->RefineCandidate(knob_->ValueBool("memo_failed"), false);

  // save iroot db
  iroot_db_->Save(knob_->ValueStr("iroot_out"), sinfo_);
  // save memoization db
  memo_->Save(knob_->ValueStr("memo_out"), sinfo_);
  // save shared instruction db
  sinst_db_->Save(knob_->ValueStr("sinst_out"), sinfo_);
}

} // namespace idiom
