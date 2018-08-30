#include "stdafx.h"

#include "simulation.h"

#include <random>


void Simulation::initOGVs(Timestamp duration)
{
  oceanQueue = std::vector<OceanVeichle::Ptr>();
  oceanQueue.emplace_back(std::make_shared<OceanVeichle>()); // first OGV is ready
  oceanQueue.front()->id = "OGV #0";

  std::default_random_engine generator;
  std::normal_distribution<float> dist(0.f, static_cast<float>(ogvPeriodStddev));
  int counter = 0;

  for (auto ogvArrival = ogvPeriod; ogvArrival <= duration + ogvPeriod; ogvArrival += ogvPeriod)
  {
    auto newOgv = std::make_shared<OceanVeichle>();
    newOgv->localTimer = static_cast<Timestamp>(static_cast<float>(ogvArrival) + dist(generator));
    newOgv->rememberState(Ship::State::GOING_LOAD);
    newOgv->id = "OGV #" + std::to_string(++counter);
    oceanQueue.push_back(newOgv);
  }
}

OceanVeichle::Ptr Simulation::getClosestOgv() 
{
  //std::cerr << "\nCurrent: " << currentTime << std::endl;

  for (auto ogv : oceanQueue)
  {
   // std::cerr << ogv->id << " - " << ogv->localTimer<<" - " << static_cast<int>(ogv->getState()) << std::endl;
    if ((ogv->getState() == Ship::State::LOADING || ogv->getState() == Ship::State::GOING_LOAD))
      return ogv;
  }
  throw std::runtime_error("No more OGVs!");
}




float Simulation::run(Timestamp duration)
{
  if (!riefTide)
    throw std::runtime_error("Tide table was not setted");

  initOGVs(duration);

  ammount = 0.f;
  int counter = 0;
  for (auto &context : ships)
  {
    context.ship->clear();
    if (counter++ % 2)
      context.ship->rememberState(Ship::State::GOING_LOAD, "Teleport");
  }
  while (process(getNextShip()) < duration) {}

  return ammount;
}


ShipContext & Simulation::getNextShip()
{
  if (ships.empty())
    throw std::runtime_error("No ships!");
  auto minTimestampShip = ships.begin();
  for (auto i = ships.begin(); i != ships.end(); ++i)
    if (i->ship->localTimer < minTimestampShip->ship->localTimer)
      minTimestampShip = i;
  if (minTimestampShip->orders.empty())
  {
    minTimestampShip->orders.emplace();
    std::cerr << "Adding default order." << std::endl;
  }
  return *minTimestampShip;
}

void Simulation::load(IBarge::Ptr ship, const LoadingOrder& order)
{
  ship->load(order.loadCargo);


  const auto loadingDuration = 2 * ship->dockingTime + static_cast<Timestamp>(order.loadCargo / order.loadIntensity);
  //std::cerr << "loadingDuration " << loadingDuration << std::endl;
  ship->rememberState(Ship::State::WAITING, "loader busy");
  ship->localTimer += loader.timeToUnlock(ship->localTimer); // waiting for free loader
  loader.lock(ship->localTimer, loadingDuration);
  ship->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(order.loadCargo, 0) + " on " + to_string_with_precision(60 * order.loadIntensity, 0) + "/h");
  ship->localTimer += loadingDuration; // prepairing & loading
  ship->rememberState(Ship::State::GOING_UNLOAD, "draft " + to_string_with_precision(ship->draft(), 3));

}


Timestamp Simulation::ogvWaitingTime(float cargo, Timestamp time)
{
  OceanVeichle::Ptr ogv = getClosestOgv();
  if (ogv->localTimer > time)
    return (ogv->localTimer - time) + ogvWaitingTime(cargo, ogv->localTimer);
  if (ogv->getState() == Ship::State::LOADING)
  {
    if (ogv->capacity > ogv->cargo + cargo)
    {
      ogv->load(cargo);
      ogv->localTimer = time;
      ogv->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(cargo, 0) +", total " + to_string_with_precision(ogv->cargo, 0));
      return 0;
    }
    else
    {
      float cargoThatFits = ogv->capacity - ogv->cargo;
      ogv->load(cargoThatFits);
      ogv->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(cargoThatFits, 0) + ", total " + to_string_with_precision(ogv->cargo, 0));

      ogv->localTimer += ogvChangingTime;
      ogv->rememberState(Ship::State::GOING_UNLOAD, "full, going away");

      return ogvWaitingTime(cargo - cargoThatFits, time + ogvChangingTime);
    }
  }
  
  if (ogv->getState() == Ship::State::GOING_LOAD)
  {
    Timestamp waitingTime = ogv->localTimer > time ? ogv->localTimer - time : 0;
    ogv->rememberState(Ship::State::LOADING, "docking");
    ogv->localTimer += ogvChangingTime;

    return waitingTime + ogvWaitingTime(cargo, time + waitingTime);

  }
  else throw std::logic_error("Impossible state");
}

void Simulation::unload(IBarge::Ptr ship)
{
  const float cargo = ship->unload();
  OceanVeichle ogv;
  

  auto unloadingDuration = 2 * ship->dockingTime + static_cast<Timestamp>(cargo / unloadingSpeed);
  ship->rememberState(Ship::State::WAITING, "unloader busy");
  ship->localTimer += unloader.timeToUnlock(ship->localTimer); // waiting for free unloader

  auto waitForOGV = ogvWaitingTime(cargo, ship->localTimer);

  unloader.lock(ship->localTimer, waitForOGV + unloadingDuration);

  ship->rememberState(Ship::State::WAITING, "no OGV on unloader");
  ship->localTimer += waitForOGV; // prepairing & unloading

  ship->rememberState(Ship::State::UNLOADING, "collected " + to_string_with_precision(ammount + cargo, 0));
  ship->localTimer += unloadingDuration; // prepairing & unloading
  ship->rememberState(Ship::State::GOING_LOAD, "draft " + to_string_with_precision(ship->draft(), 3));

  ammount += cargo;
}


void Simulation::passRief(IBarge::Ptr ship, bool fromCoast)
{
  if (!riefTide)
    throw std::runtime_error("riefTide not setted");

  const float toRief = fromCoast ? distLoadToRief : distUnloadToRief;
  const float fromRief = fromCoast ? distUnloadToRief : distLoadToRief;
  ship->localTimer += static_cast<Timestamp>(toRief / ship->velocity());
  const auto currentTideString = [&ship](TideTable::ConstPtr riefTide) {return  to_string_with_precision(riefTide->draft(ship->localTimer), 3); };
  ship->rememberState(Ship::State::WAITING, "low tide " + currentTideString(riefTide));
  ship->localTimer += riefTide->timeToPossibleDraft(ship->localTimer, ship->draft());
  ship->rememberState(Ship::State::RIEF_PASSED, " tide  " + currentTideString(riefTide) + (fromCoast ? ", go to sea" : ", go to port"));
  ship->localTimer += static_cast<Timestamp>(fromRief / ship->velocity());
}

void Simulation::dockBTC(IBarge::Ptr ship)
{
  ship->localTimer += ship->riefPreparingTime;
  if (ship->riefPreparingTime)
	  ship->rememberState(Ship::State::DOCKING, "BTC assembling");
}

Timestamp Simulation::process(ShipContext& context)
{
  auto ship = context.ship;
 

  if (ship->getState() == Ship::State::LOADING)
  {
    if (context.orders.empty())
      throw std::runtime_error("Order queue is empty");
    load(ship, context.orders.front());
    context.orders.pop();
  }
  else if (ship->getState() == Ship::State::GOING_UNLOAD)
  {
    passRief(ship, true);
	dockBTC(ship);
    ship->rememberState(Ship::State::UNLOADING);
  }
  else if (ship->getState() == Ship::State::UNLOADING)
    unload(ship);
  else if (ship->getState() == Ship::State::GOING_LOAD)
  {
    dockBTC(ship);
    passRief(ship, false);
    ship->rememberState(Ship::State::LOADING);
  }
  else throw std::runtime_error("Impossible state");
  return ship->localTimer;
}

