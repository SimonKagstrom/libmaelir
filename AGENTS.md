# Introduction

Welcome, agents, to the libmaelir repository! libmaelir is used as a submodule for other
projects, and these rules typically apply to them as well.

## Editing
* Use the vscode API, if running via vscode, so that your reads and edits can be seen
* If you, as an agent, runs the Qt application, please run it off-screen and take screenshots
  so that it doesn't pop up a window

## General rules
* C++ only for the project code. C etc is only allowed on human intervention.
  - Currently C++23 is used
  - Filenames are .cc and .hh
  - #pragma once is used for headers
  - Member variables use the m_ prefix
  - Constants use the k prefix
* Tools may use Python etc
* Unless it targets e.g., esp-idf specifically, code should build for unit tests, Qt and
  the target
* The code should be unit tested, wherever possible

## State machines
* State machines are generated from .md files
* Enter() and Exit() for each state is where the work should be done
* The Evaluate() function should in general not do much work, but simply check if the next
  state should be entered. There are exceptions

## Threads
* The threading model uses BaseThread and PooledThreadBase, and all threads should inherit these
  - PooledThreadBase saves stack space since it's built on a single FreeRTOS thread
* PooledThreadBase should be used for tasks which are non-time sensitive and don't block
  the CPU for long periods of time

## Thread communication
Threads communicate via a "blackboard" mechanism, and through sending messages - not via
add-hoc shared state.

* Application state: A shared database which threads can read, write and get notifications from
  - These are defined in YML files and generated
* Postoffice: A message system for events, where messages are sent to whoever listens (or none,
  if there are no listeners)

# Hardware abstraction
* New hardware drivers go into hal/i_*.hh as an abstract interface
* Implementation is in esp32 and qt
* Regular code use only the interface
* Trompeloeil mocks of the interface is added for the unit tests

## Unit tests
* Use BDD-style tests where appropriate
* Use TEST_SUITE_BEGIN/END to define test suites
* Use the ThreadFixture / PooledThreadFixture for deterministic tests
* Use REQUIRE if later checks depend on it (e.g., that a pointer is valid), and CHECK otherwise
  - REQUIRE will stop the test, CHECK will allow it to continue
