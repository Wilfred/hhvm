 /*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-present Facebook, Inc. (http://www.facebook.com)  |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifdef HHVM_TAINT

#include <fstream>
#include <sstream>

#include <folly/Singleton.h>
#include <folly/dynamic.h>

#include "hphp/runtime/base/execution-context.h"
#include "hphp/runtime/base/init-fini-node.h"
#include "hphp/runtime/base/runtime-option.h"

#include "hphp/runtime/vm/taint/configuration.h"
#include "hphp/runtime/vm/taint/state.h"

#include "hphp/util/process.h"
#include "hphp/util/trace.h"

namespace HPHP {
namespace taint {

TRACE_SET_MOD(taint);

std::string Path::jsonLine() const {
  const Func* last = nullptr;

  std::vector<Hop> hops;
  const Path* path = this;
  while (path != nullptr) {
    auto hop = path->hop;
    if (hop.from != nullptr || hop.to != nullptr) {
      hops.push_back(hop);
    }
    path = path->parent;
  }

  std::stringstream stream;
  stream << "{\"hops\": [";
  for (int i = hops.size() - 1; i >= 0; i--) {
    auto hop = hops[i];

    if (last != hop.from && hop.from != nullptr) {
      stream << "\"" << hop.from->fullName()->data() << "\", ";
    }
    last = hop.from;

    if (last != hop.to && hop.to != nullptr) {
      stream << "\"" << hop.to->fullName()->data() << "\"";
    }
    last = hop.to;

    if (i != 0) {
      stream << ", ";
    }
  }
  stream << "]}";
  return stream.str();
}

Path::Path() : hop{nullptr, nullptr}, parent(nullptr) {}

void destructPath(void* p) {
  auto path = (Path*)p;
  path->~Path();
}

Path* Path::origin(PathArena* arena, Hop hop) {
  Path* path = arena->allocD<Path>(&destructPath);
  if (!path) {
    return path;
  }
  path->hop = hop;
  path->parent = nullptr;
  return path;
}

Path* Path::to(PathArena* arena, Hop hop) const {
  Path* child = arena->allocD<Path>(&destructPath);
  if (!child) {
    return child;
  }
  child->hop = hop;
  child->parent = this;
  return child;
}

std::ostream& operator<<(std::ostream& out, const HPHP::taint::Value& value) {
  if (!value) {
    return out << "_";
  } else {
    return out << "S";
  }
}

void Stack::push(Value value) {
  m_stack.push_back(value);
}

void Stack::pushFront(Value value) {
  m_stack.push_front(value);
}

Value Stack::top() const {
  return peek(0);
}

Value Stack::peek(int offset) const {
  // TODO(T93491972): replace with assertions once we can run the integration
  // tests.
  if (m_stack.size() <= offset) {
    FTRACE(
        3,
        "taint: (WARNING) called `Stack::peek({})` on stack of size {}\n",
        offset,
        m_stack.size());
    return nullptr;
  }
  return m_stack[m_stack.size() - 1 - offset];
}

void Stack::pop(int n) {
  if (m_stack.size() < n) {
    FTRACE(
        3,
        "taint: (WARNING) called `Stack::pop({})` on stack of size {}\n",
        n,
        m_stack.size());
    n = m_stack.size();
  }

  for (int i = 0; i < n; i++) {
    m_stack.pop_back();
  }
}

void Stack::popFront() {
  if (m_stack.empty()) {
    FTRACE(3, "taint: (WARNING) called `Stack::popFront()` on empty stack\n");
    return;
  }

  m_stack.pop_front();
}

void Stack::replaceTop(Value value) {
  if (m_stack.empty()) {
    FTRACE(3, "taint: (WARNING) called `Stack::replaceTop()` on empty stack\n");
    return;
  }
  m_stack.back() = value;
}

size_t Stack::size() const {
  return m_stack.size();
}

std::string Stack::show() const {
  std::stringstream stream;
  for (const auto value : m_stack) {
    stream << value << " ";
  }
  stream << "(t)";
  return stream.str();
}

void Stack::clear() {
  m_stack.clear();
}

void Heap::set(tv_lval typedValue, Value value) {
  m_heap[std::move(typedValue)] = value;
}

Value Heap::get(const tv_lval& typedValue) const {
  auto value = m_heap.find(typedValue);
  if (value != m_heap.end()) {
    return value->second;
  }

  return nullptr;
}

void Heap::clear() {
  m_heap.clear();
}

namespace {

struct SingletonTag {};

InitFiniNode s_stateInitialization(
    []() { State::instance->initialize(); },
    InitFiniNode::When::RequestStart);

InitFiniNode s_stateTeardown(
    []() { State::instance->teardown(); },
    InitFiniNode::When::RequestFini);

} // namespace

rds::local::RDSLocal<State, rds::local::Initialize::FirstUse> State::instance;

State::State() : arena(std::make_unique<PathArena>()) {}

void State::initialize() {
  m_request_start = std::chrono::system_clock::now();
  FTRACE(1, "taint: initializing state\n");

  stack.clear();
  heap.clear();
  paths.clear();
  arena = std::make_unique<PathArena>();
  m_function_metadata = Configuration::get()->functionMetadata();

  // Stack is initialized with 4 values before any operation happens.
  // We don't care about these values but mirroring simplifies
  // consistency checks.
  for (int i = 0; i < 4; i++) {
    stack.push(nullptr);
  }
}

namespace {

folly::dynamic requestMetadata(
    std::chrono::time_point<std::chrono::system_clock> requestStart) {
  folly::dynamic metadata = folly::dynamic::object;
  metadata["metadata"] = true;

  metadata["mimeType"] = g_context->getMimeType();
  metadata["workingDirectory"] = g_context->getCwd();

  auto requestUrl = g_context->getRequestUrl();

  if (RO::EvalTaintLogRequestURLs) {
    metadata["requestUrl"] = requestUrl;
  }

  auto commandLine = Process::GetCommandLine(getpid());
  metadata["commandLine"] = commandLine;

  std::chrono::time_point<std::chrono::system_clock> endTime =
      std::chrono::system_clock::now();
  metadata["requestStartTime"] =
      std::chrono::duration_cast<std::chrono::seconds>(
          requestStart.time_since_epoch())
          .count();
  metadata["requestEndTime"] = std::chrono::duration_cast<std::chrono::seconds>(
                                   endTime.time_since_epoch())
                                   .count();
  metadata["requestDurationMs"] =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          endTime - requestStart)
          .count();

  auto timeForHash = std::chrono::duration_cast<std::chrono::milliseconds>(
                         requestStart.time_since_epoch())
                         .count();
  metadata["identifier"] = requestUrl != ""
      ? folly::sformat(
            "request-{}",
            std::hash<std::string>()(
                folly::sformat("{}-{}", requestUrl, timeForHash)))
      : folly::sformat(
            "script-{}",
            std::hash<std::string>()(
                folly::sformat("{}-{}", commandLine, timeForHash)));

  return metadata;
}

} // namespace

void State::teardown() {
  auto metadata = requestMetadata(m_request_start);
  auto identifier = metadata["identifier"].asString();
  FTRACE(1, "taint: processed request `{}`\n", identifier);

  auto outputDirectory = Configuration::get()->outputDirectory;
  if (!outputDirectory) {
    // Print to stderr, useful for integration tests.
    for (auto& path : paths) {
      std::cerr << path->jsonLine() << std::endl;
    }
    return;
  }

  auto outputPath = *outputDirectory + "/output-" + identifier + ".json";
  if (paths.empty()) {
    FTRACE(1, "taint: no data flows found in request `{}`\n", identifier);
    return;
  }

  FTRACE(1, "taint: writing results to {}\n", outputPath);
  try {
    std::ofstream output;
    output.open(outputPath);
    output << folly::toJson(metadata) << std::endl;
    for (auto& path : paths) {
      output << path->jsonLine() << std::endl;
    }
    output.close();
  } catch (std::exception& exception) {
    throw std::runtime_error("unable to write to `" + outputPath + "`");
  }
}

std::vector<Source> State::sources(const Func* func) {
  if (!m_function_metadata) {
    m_function_metadata = Configuration::get()->functionMetadata();
    if (!m_function_metadata) {
      return {};
    }
  }
  return m_function_metadata->sources(func);
}

std::vector<Sink> State::sinks(const Func* func) {
  if (!m_function_metadata) {
    m_function_metadata = Configuration::get()->functionMetadata();
    if (!m_function_metadata) {
      return {};
    }
  }
  return m_function_metadata->sinks(func);
}

} // namespace taint
} // namespace HPHP

#endif
