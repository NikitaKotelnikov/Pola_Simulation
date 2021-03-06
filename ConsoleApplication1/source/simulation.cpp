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
	blockers[ships.front().ship] = loader;

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

	riverTows.posA = Ship::State::LOADING;
	riverTows.posB = Ship::State::UNLOADING;
	riverTows.dist = (useRiverTows ? distLoadToDrop : totalDist);

  seaTows.posA = Ship::State::LOADING;
  seaTows.posB = Ship::State::UNLOADING;
  seaTows.dist = totalDist - distLoadToDrop;

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
	//auto loader = blockers.at(ship);
	waitForResource(loader, ship);
	ship->load(order.loadCargo);

	const auto loadingDuration = static_cast<Timestamp>(order.loadCargo / order.loadIntensity);

	loader->lock(ship->localTimer, loadingDuration + ship->dockingTime + ship->undockingTime, ship->id);
	ship->rememberState(Ship::State::DOCKING, "docking to "+ loader->id);
	ship->localTimer += ship->dockingTime;
	ship->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(order.loadCargo, 0) + " on " + to_string_with_precision(60 * order.loadIntensity, 0) + "/h");
	ship->localTimer += loadingDuration; // prepairing & loading
	ship->rememberState(Ship::State::DOCKING, "undocking from " + loader->id);
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

      if (ogv->capacity - ogv->cargo < ogvReserve)
         ogv->rememberState(Ship::State::GOING_UNLOAD, "almost full, going away");

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

	auto unloader = blockers[barge];


	waitForResource(unloader, barge);
	
	const auto lockStart = barge->localTimer;
	auto unloadingDuration = 0; // static_cast<Timestamp>(cargo / unloadingSpeed);
	const auto addToUnloadingTime = [&barge, &unloadingDuration](Timestamp time, Ship::State state, std::string comment)
	{
		barge->rememberState(state, comment);
		barge->localTimer += time;
		unloadingDuration += time;
	};

	if (!barge->towable)
		addToUnloadingTime(barge->dockingTime, Ship::State::DOCKING, "docking to " + unloader->id);

	auto waitForOGV = ogvWaitingTime(cargo, barge->localTimer);
	if (waitForOGV)
		addToUnloadingTime(waitForOGV, Ship::State::WAITING, "no OGV on unloader");

	const auto pureUnloadingTime = static_cast<Timestamp>(cargo / unloadingSpeed);
	const auto unloadingPowerStr = to_string_with_precision(unloadingSpeed * 60, 0)  + "/h";
	addToUnloadingTime(pureUnloadingTime, Ship::State::UNLOADING, "unloading on " + unloadingPowerStr);

	if (!barge->towable)
		addToUnloadingTime(barge->undockingTime, Ship::State::DOCKING, "undocking from " + unloader->id);

	unloader->lock(lockStart, unloadingDuration, barge->id);
	barge->rememberState(Ship::State::UNLOADING, "collected " + to_string_with_precision(ammount + cargo, 0));
	ammount += cargo;
	barge->rememberState(Ship::State::GOING_LOAD, "Going to the next raid");
}


Tow::Ptr findFreeTow(const TugPositions& pool, Ship::State pos)
{
	for (auto tow : pool.positions)
		if (tow.second == pos)
			return tow.first;
	return nullptr;
}

Tow::Ptr moveTows(TugPositions& pool)
{
	if (pool.positions.empty())
		throw std::runtime_error("Pool is empty!");
	Tow::Ptr minTimestampedTow = pool.positions.begin()->first;
	for (auto tow : pool.positions)
		if (tow.first->localTimer < minTimestampedTow->localTimer)
			minTimestampedTow = tow.first;

	Ship::State &currentPos = pool.positions.at(minTimestampedTow);
	if (currentPos == pool.posA) currentPos = pool.posB;
	else if (currentPos == pool.posB) currentPos = pool.posA;
	else throw std::logic_error("Invalid position! " + std::to_string(static_cast<int>(currentPos)));

	minTimestampedTow->rememberState(Ship::State::MOVING, "Moving to a waiting barge...");
	minTimestampedTow->move(pool.dist);
	return minTimestampedTow;
}

void Simulation::goUnloading(IBarge::Ptr barge)
{
	const float distRiefToUnload = totalDist - distLoadToRief;
  const float distRiefToChange = distLoadToDrop - distLoadToRief;
  const float distChangeToUnloading = totalDist - distLoadToDrop;

	if (barge->towable)
	{
		Tow::Ptr tug1 = summonTow(riverTows, Ship::State::LOADING, barge);
		barge->rememberState(Ship::State::GOING_UNLOAD, "Draft " + to_string_with_precision(barge->draft(), 3));
		passRief(tug1, distLoadToRief, useRiverTows ? distRiefToChange : distRiefToUnload);
    riverTows.positions[tug1] = Ship::State::UNLOADING;

    Tow::Ptr tug2;
    if (useRiverTows)
    {
      tug1->dropBarges(0);

      tug2 = summonTow(seaTows, Ship::State::LOADING, barge);
      // Tow::Ptr tug2 = tug1; //summonTow(seaTows, Ship::State::LOADING, barge);

      tug1->move(distChangeToUnloading);

    }
    else tug2 = tug1;

		if (ballanceUnloaders(tug2->localTimer))
		{
			auto barges = tug2->dropBarges(0);
			tug2->localTimer += ogvChangingTime;
			tug2->rememberState(Ship::State::DOCKING, "Reberting an UNLOADER to a new OGV");
			for (const auto barge : barges)
				tug2->towBarge(barge, 0);
		}

		const auto unloader = getUnloader(tug2->localTimer);
		blockers[barge] = unloader;

		waitForResource(unloader, tug2);
		tug2->synchronizeTimestamps();
		
		const auto lockStart = tug2->localTimer;
		tug2->dropBarges(barge->dockingTime);
		const auto lockEnd = tug2->localTimer;
    
	  if (useRiverTows)
      seaTows.positions[tug2] = Ship::State::UNLOADING;

		//unloader->lock(lockStart, lockEnd - lockStart, tug->id);
	}
	else
	{
		passRief(barge, distLoadToRief, distRiefToUnload);
		auto unloader = getUnloader(barge->localTimer);
		waitForResource(unloader, barge);
		blockers[barge] = unloader;
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

Tow::Ptr Simulation::getFreeTug(TugPositions& pool, Ship::State requiredState)
{
	if (requiredState != pool.posA && requiredState != pool.posB)
		throw std::logic_error("Invalid state");
	Tow::Ptr freeTug = findFreeTow(pool, requiredState);
	if (!freeTug)
		freeTug = moveTows(pool);
	return freeTug;
}

Tow::Ptr Simulation::summonTow(TugPositions& pool, Ship::State requiredState, IBarge::Ptr barge)
{
	Tow::Ptr freeRiverTow = getFreeTug(pool, requiredState);
	barge->rememberState(Ship::State::WAITING, "Waiting for tow " + freeRiverTow->id, true);
//	freeRiverTow->rememberState(Ship::State::DOCKING, "Ready on " + Ship::printState(requiredState));
	freeRiverTow->towBarge(barge, barge->undockingTime);
	return freeRiverTow;
}

void Simulation::waitForResource(UsedResource::Ptr loader, Ship::Ptr ship)
{
	//if (!loader->isFreeAfter(ship->localTimer))
	//	throw std::runtime_error("Somewhat will lock the resource after!");
	const auto waitingTime = loader->timeToUnlock(ship->localTimer, ship->id);
	if (waitingTime)
		ship->rememberState(Ship::State::WAITING, loader->id + " busy");
	ship->localTimer += waitingTime;
}


void Simulation::goLoading(IBarge::Ptr barge)
{
  const float distRiefToUnload = totalDist - distLoadToRief;
  const float distRiefToChange = distLoadToDrop - distLoadToRief;
  const float distChangeToUnloading = totalDist - distLoadToDrop;


  if (barge->towable)
	{
    if (useRiverTows)
    {
      Tow::Ptr tug2 = summonTow(seaTows, Ship::State::UNLOADING, barge);
      tug2->move(distChangeToUnloading);
      tug2->dropBarges(barge->dockingTime);
      seaTows.positions[tug2] = Ship::State::LOADING;
    }

		//if (blockers.count(barge))
		//	blockers[barge]->lock(barge->localTimer, barge->undockingTime, tug->id);

		barge->rememberState(Ship::State::GOING_LOAD, "draft " + to_string_with_precision(barge->draft(), 3));

    Tow::Ptr tug1 = summonTow(riverTows, Ship::State::UNLOADING, barge);

		passRief(tug1, (useRiverTows ? distRiefToChange : distRiefToUnload), distLoadToRief);

		tug1->synchronizeTimestamps();
		waitForResource(loader, barge);


		const auto lockStart = tug1->localTimer;
		tug1->dropBarges(barge->dockingTime);
		const auto lockEnd = tug1->localTimer;

		blockers[barge] = loader;
		//loader->lock(lockStart, lockEnd - lockStart, tug->id);
		riverTows.positions[tug1] = Ship::State::LOADING;
	}
	else
	{
		passRief(barge, distRiefToUnload, distLoadToRief);
		waitForResource(loader, barge);
		blockers[barge] = loader;
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

