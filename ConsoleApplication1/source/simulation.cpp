#include "stdafx.h"

#include "simulation.h"

#include <random>


void Simulation::initOGVs(Timestamp duration)
{
	oceanQueue.clear();


	std::default_random_engine generator;
	std::normal_distribution<float> dist(0.f, static_cast<float>(ogvPeriodStddev));
	int counter = 0;

	oceanQueue.emplace_back(std::make_shared<OceanVeichle>());
	oceanQueue.back()->rememberState(Ship::State::LOADING);
	oceanQueue.back()->id = "OGV #" + std::to_string(++counter);

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


	ammount = 0.f;
	int counter = 0;
	for (auto &context : ships)
	{
		context.ship->clear();
		if (counter++)
			context.ship->rememberState(Ship::State::GOING_LOAD, "Teleport");
	}
	counter = 0;
	unloaders.clear();
	/*for (int u : {0, 1})
	{*/
		unloaders.push_back(std::make_shared<UsedResource>());
		unloaders.back()->id = "UNLOADER #" + std::to_string(++counter);
		unloaderToOGV[unloaders.back()] = nullptr;
	//}
	loader->id = "LOADER";
	initOGVs(duration);

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
  waitForResource(loader, ship);
	ship->load(order.loadCargo);

	const auto loadingDuration = static_cast<Timestamp>(order.loadCargo / order.loadIntensity);
  const auto loadingStartTime = ship->localTimer;
	loader->lock(loadingStartTime, loadingDuration + ship->dockingTime + ship->undockingTime, ship->id);
  ship->rememberState(Ship::State::DOCKING, "docking");
  ship->localTimer += ship->dockingTime;
	ship->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(order.loadCargo, 0) + " on " + to_string_with_precision(60 * order.loadIntensity, 0) + "/h");
	ship->localTimer += loadingDuration; // prepairing & loading
  ship->rememberState(Ship::State::DOCKING, "undocking");
	ship->localTimer += ship->undockingTime;
	ship->rememberState(Ship::State::GOING_UNLOAD, "draft " + to_string_with_precision(ship->draft(), 3));
}

bool Simulation::ballanceUnloaders(Timestamp currentTime)
{
	int ballanced = 0;
	for (auto unloader : unloaders)
	{
		OceanVeichle::ConstPtr connectedOGV = unloaderToOGV[unloader];
		if (!connectedOGV || connectedOGV->getState() != Ship::State::LOADING)
		{
			connectedOGV = getClosestOgv();
			unloader->lock(currentTime, ogvChangingTime, connectedOGV->id);
			unloaderToOGV[unloader] = connectedOGV;
			return true;
		}
	}
	return false;
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
			ogv->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(cargo, 0) + ", total " + to_string_with_precision(ogv->cargo, 0));
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

void Simulation::unload(IBarge::Ptr barge)
{

	const float cargo = barge->unload();
	OceanVeichle ogv;


	auto unloadingDuration = static_cast<Timestamp>(cargo / unloadingSpeed);

	const auto lockingTime = barge->localTimer - barge->dockingTime;
	auto unloader = getUnloader(lockingTime);
  waitForResource(unloader, barge);


	auto waitForOGV = ogvWaitingTime(cargo, barge->localTimer);


	//unloader->lock(lockingTime, waitForOGV + unloadingDuration + barge->dockingTime + barge->undockingTime, "locked by " + barge->id);

	if (waitForOGV)
		barge->rememberState(Ship::State::WAITING, "no OGV on unloader");
	barge->localTimer += waitForOGV;
  barge->rememberState(Ship::State::DOCKING, "docking");
  barge->localTimer += barge->dockingTime;

	unloader->lock(lockingTime, waitForOGV + unloadingDuration + barge->dockingTime + barge->undockingTime, barge->id);
	barge->rememberState(Ship::State::UNLOADING, "unloading to " + unloader->id);
	barge->localTimer += unloadingDuration;
  barge->rememberState(Ship::State::UNLOADING, "collected " + to_string_with_precision(ammount + cargo, 0));
  barge->rememberState(Ship::State::DOCKING, "docking");
  barge->localTimer += barge->undockingTime;

	barge->rememberState(Ship::State::GOING_LOAD, "draft " + to_string_with_precision(barge->draft(), 3));

	ammount += cargo;
}


Tow::Ptr findFreeTow(std::map<Tow::Ptr, Ship::State>& pool, Ship::State pos)
{
	for (auto tow : pool)
		if (tow.second == pos)
			return tow.first;
	return nullptr;
}

Tow::Ptr moveTows(std::map<Tow::Ptr, Ship::State>& pool, Ship::State posA, Ship::State posB, float dist)
{
	if (pool.empty())
		throw std::runtime_error("Pool is empty!");
	Tow::Ptr minTimestampedTow = pool.begin()->first;
	for (auto tow : pool)
		if (tow.first->localTimer < minTimestampedTow->localTimer)
			minTimestampedTow = tow.first;

	Ship::State &currentPos = pool.at(minTimestampedTow);
	if (currentPos == posA) currentPos = posB;
	else if (currentPos == posB) currentPos = posA;
	else throw std::logic_error("Invalid position! "+std::to_string(static_cast<int>(currentPos)));

	minTimestampedTow->rememberState(Ship::State::MOVING, "Moving to a waiting barge...");
	minTimestampedTow->move(dist);
	return minTimestampedTow;
}

void Simulation::goUnloading(IBarge::Ptr barge)
{
  float distRiefToUnload = totalDist - distLoadToRief;

  
	if (barge->towable)
	{

		Tow::Ptr freeRiverTow = getFreeRiverTow(Ship::State::GOING_UNLOAD);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeRiverTow->id);

		freeRiverTow->towBarge(barge, barge->unanchoringTime);
		barge->rememberState(Ship::State::GOING_UNLOAD, "draft " + to_string_with_precision(barge->draft(), 3));

		
		passRief(freeRiverTow, distLoadToRief, distRiefToUnload);

    const auto unloader = getUnloader(freeRiverTow->localTimer);
    freeRiverTow->synchronizeTimestamps();
    waitForResource(unloader, freeRiverTow);
    freeRiverTow->synchronizeTimestamps();

	/*	auto unloader = getUnloader(freeRiverTow->localTimer);
		const auto waitingTime = unloader->timeToUnlock(freeRiverTow->localTimer);
		if (waitingTime)
			barge->rememberState(Ship::State::WAITING, unloader->id + " unloader busy");
    freeRiverTow->localTimer += waitingTime;*/


		const bool unloadersToBallnce = ballanceUnloaders(freeRiverTow->localTimer);
		if (unloadersToBallnce)
		{
      freeRiverTow->localTimer += barge->anchoringTime + ogvChangingTime + barge->unanchoringTime;
      freeRiverTow->rememberState(Ship::State::DOCKING, "Reberting " + unloader->id);
		}
    unloader->lock(barge->localTimer, barge->dockingTime + barge->undockingTime, barge->id);
    freeRiverTow->dropBarges(barge->anchoringTime);
    freeRiverTow->localTimer += barge->dockingTime;

		riverTows[freeRiverTow] = Ship::State::WAITING;
	}
	else
	{
		passRief(barge, distLoadToRief, distRiefToUnload);
    auto unloader = getUnloader(barge->localTimer);
    waitForResource(unloader, barge);
    unloader->lock(barge->localTimer, barge->dockingTime + barge->undockingTime, barge->id);
	}

	barge->rememberState(Ship::State::UNLOADING);
}

UsedResource::Ptr Simulation::getUnloader(Timestamp currentTime) const
{
	std::map<Timestamp, UsedResource::Ptr> waitTimeRanged;
	for (auto unloader : unloaders)
		waitTimeRanged.emplace(unloader->timeToUnlock(currentTime, ""), unloader);
	return waitTimeRanged.begin()->second;
}

Tow::Ptr Simulation::getFreeRiverTow(Ship::State requiredState)
{
	if (requiredState != Ship::State::GOING_UNLOAD && requiredState != Ship::State::WAITING)
		throw std::logic_error("Invalid state");
	Tow::Ptr freeRiverTow = findFreeTow(riverTows, Ship::State::GOING_UNLOAD);
	if (!freeRiverTow)
		freeRiverTow = moveTows(riverTows, Ship::State::GOING_UNLOAD, Ship::State::WAITING, totalDist);
	return freeRiverTow;
}
//Tow::Ptr Simulation::getFreeSeaTow(Ship::State requiredState)
//{
//	if (requiredState != Ship::State::GOING_LOAD && requiredState != Ship::State::WAITING)
//		throw std::logic_error("Invalid state");
//
//	float distDropToUnloader = totalDist - distLoadToDrop;
//	Tow::Ptr freeSeaTow = findFreeTow(seaTows, requiredState);
//	if (!freeSeaTow)
//		freeSeaTow = moveTows(seaTows, Ship::State::WAITING, Ship::State::GOING_LOAD, distDropToUnloader);
//	return freeSeaTow;
//}

void Simulation::waitForResource(UsedResource::Ptr loader, Ship::Ptr ship)
{
  const auto waitingTime = loader->timeToUnlock(ship->localTimer, ship->id);
  if (waitingTime)
    ship->rememberState(Ship::State::WAITING, "(un)loader busy");
  ship->localTimer += waitingTime;
}


void Simulation::goLoading(IBarge::Ptr barge)
{
  const float distUnloadToRief = totalDist - distLoadToRief;

 

	if (barge->towable)
	{
		//Tow::Ptr freeSeaTow = getFreeSeaTow(Ship::State::GOING_LOAD);
		//barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeSeaTow->id, true);

		//const float distDropToUnloader = totalDist - distLoadToDrop;
		//freeSeaTow->towBarge(barge, barge->undockingTime);
		//freeSeaTow->move(distDropToUnloader);
		//freeSeaTow->dropBarges(barge->anchoringTime);

		//seaTows[freeSeaTow] = Ship::State::WAITING;

		Tow::Ptr freeRiverTow = getFreeRiverTow(Ship::State::WAITING);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeRiverTow->id, true);

		freeRiverTow->towBarge(barge, barge->unanchoringTime);
		barge->rememberState(Ship::State::GOING_LOAD, "draft " + to_string_with_precision(barge->draft(), 3));

		//if (distUnloadToRief < 0)
		//	throw std::runtime_error("Can't drop before rief");
		passRief(freeRiverTow, distLoadToRief, distUnloadToRief);

		freeRiverTow->synchronizeTimestamps();
    waitForResource(loader, barge);

    loader->lock(barge->localTimer, barge->dockingTime + barge->undockingTime, barge->id);


		freeRiverTow->dropBarges(barge->anchoringTime);
		riverTows[freeRiverTow] = Ship::State::GOING_UNLOAD;
	}
	else
	{

		passRief(barge, distUnloadToRief, distLoadToRief);
    waitForResource(loader, barge);
    loader->lock(barge->localTimer, barge->dockingTime + barge->undockingTime, barge->id);

	}

	barge->rememberState(Ship::State::LOADING);
}

void Simulation::passRief(Ship::Ptr ship, float toRief, float fromRief)
{
	if (!riefTide)
		throw std::runtime_error("riefTide not setted");

	ship->move(toRief);
	const auto currentTideString = [&ship](TideTable::ConstPtr riefTide) { return  to_string_with_precision(riefTide->draft(ship->localTimer), 3); };
	ship->rememberState(Ship::State::WAITING, "low tide " + currentTideString(riefTide));
	ship->localTimer += riefTide->timeToPossibleDraft(ship->localTimer, ship->draft());
	ship->rememberState(Ship::State::RIEF_PASSED, " tide  " + currentTideString(riefTide));
	ship->move(fromRief);
}

Timestamp Simulation::process(ShipContext& context)
{
	auto ship = context.ship;
	const float distUnloadToRief = totalDist - distLoadToRief;


	if (ship->getState() == Ship::State::LOADING)
	{
		if (context.orders.empty())
			throw std::runtime_error("Order queue is empty");
		load(ship, context.orders.front());
		context.orders.pop();
	}
	else if (ship->getState() == Ship::State::GOING_UNLOAD)
		goUnloading(ship);
	else if (ship->getState() == Ship::State::UNLOADING)
		unload(ship);
	else if (ship->getState() == Ship::State::GOING_LOAD)
		goLoading(ship);
	else throw std::runtime_error("Impossible state");
	return ship->localTimer;
}

