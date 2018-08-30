#pragma once

#include "core.h"

struct Ship
{
  enum class State { LOADING, UNLOADING, GOING_LOAD, GOING_UNLOAD, WAITING, RIEF_PASSED, DOCKING };
  using Ptr = std::shared_ptr<Ship>;
  using ConstPtr = std::shared_ptr<const Ship>;

  void rememberState(State askedState, std::string comment = "")
  {
    history[localTimer] = printState(askedState) + "\t" + comment;
    state = askedState;
  }

  template <typename T>
  void printHistory(T& stream) const
  {
    stream << "\nLog of \"" << id << "\":\n";
    for (auto &shipEvent : history)
      stream << printTime(shipEvent.first) << "\t" << shipEvent.second << "\n";
  }
  Timestamp grepHistory(const std::string &str)
  {
    Timestamp sum = 0;
    if (history.size() < 2)
      return 0;
    auto eventIterator = history.begin();
    do
    {
      const bool contains = eventIterator->second.find(str) != std::string::npos;
      const auto curr = eventIterator->first;
      if (eventIterator == history.end()) break;
      const auto next = (++eventIterator)->first;
      if (contains)
        sum += next - curr;
    } while (eventIterator != history.end());
    return sum;
  }


  void load(float cargoToLoad)
  {
    if (cargoToLoad + cargo > capacity)
      throw std::runtime_error("Ordered to load more than possible: asked " + std::to_string(cargoToLoad)+", already " + std::to_string(cargo) + " / " + std::to_string(capacity));
    cargo += cargoToLoad;
  }
  float unload()
  {
    const float cargoToUnload = cargo;
    cargo = 0;
    ++loadsCounter;
    return cargoToUnload;
  }

  State getState() const { return state; }

  void clear()
  {
    localTimer = 0;
    cargo = 0.f;
    loadsCounter = 0;
    state = State::LOADING;
    history.clear();
  }

protected:
  static std::string printState(State st)
  {
    switch (st)
    {
    case State::LOADING:      return "LOADING...";
    case State::UNLOADING:    return "UNLOADING...";
    case State::GOING_LOAD:   return "GOING_LOAD";
    case State::GOING_UNLOAD: return "GOING_UNLOAD";
    case State::WAITING:      return "WAITING...";
    case State::RIEF_PASSED:  return "RIEF_PASSED";
    case State::DOCKING:      return "DOCKING";

    default:  return "???";
    }
  }

protected:
  std::map < Timestamp, std::string > history;
  State state{ State::LOADING };

public:
  std::string id = "UNNAMED";
  Timestamp localTimer = 0;

  float capacity { 0.f };
  float cargo{ 0.f };
  int loadsCounter = 0;

};

struct IBarge : Ship
{
  using Ptr = std::shared_ptr<IBarge>;
  using ConstPtr = std::shared_ptr<const IBarge>;

  virtual float velocity() const { return cargo ? cargoVelocity : ballastVelocity; }
  virtual float draft() const { return ballastDraft + draftBonus; }

  Timestamp dockingTime{ 0 };
  Timestamp riefPreparingTime{ 0 };

protected:
  float ballastDraft {0};
  float ballastVelocity {0};
  float cargoVelocity {0};
  float draftBonus {0};

};

struct Barge : IBarge
{
  using Ptr = std::shared_ptr<IBarge>;
  using ConstPtr = std::shared_ptr<const IBarge>;


  Barge()
  {
    ballastDraft = 3.f;
    draftBonus = 0.6f;
    ballastVelocity = 6.5f / 60;
    cargoVelocity = 5.5f / 60;
    capacity = 7200.f;
    dockingTime = 30;
    riefPreparingTime = 0;
  }
  virtual ~Barge() = default;

  float draft() const override { return (cargo ? draftTable->get(cargo) : ballastDraft) + draftBonus; };

  void setDraftTable(ShipDraftTable::ConstPtr table) { draftTable = table; }

private:
  ShipDraftTable::ConstPtr draftTable{ nullptr };
};


struct BTC : IBarge
{
  BTC()
  {
    ballastDraft = 0.9f;
    cargoDraft = 3.f;
    draftBonus = 0.3f;
    ballastVelocity = 5.5f / 60;
    cargoVelocity = 4.5f / 60;
    capacity = 12000.f;
    dockingTime = 30;
    riefPreparingTime = 60;
  }
  virtual ~BTC() = default;
  float draft() const override { return cargo ? cargoDraft : ballastDraft; }

private:
  float cargoDraft;

};


struct OceanVeichle : Ship
{
  using Ptr = std::shared_ptr<OceanVeichle>;
  using ConstPtr = std::shared_ptr<const OceanVeichle>;
  OceanVeichle() 
  {
    capacity = 63e3;
  }

};

