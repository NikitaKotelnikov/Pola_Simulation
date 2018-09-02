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

	const auto loadingDuration = static_cast<Timestamp>(order.loadCargo / order.loadIntensity);

	loader.lock(ship->localTimer, loadingDuration + 2 * ship->dockingTime);
	ship->rememberState(Ship::State::LOADING, "loaded " + to_string_with_precision(order.loadCargo, 0) + " on " + to_string_with_precision(60 * order.loadIntensity, 0) + "/h");
	ship->localTimer += loadingDuration; // prepairing & loading
	if (!useTows)
		ship->localTimer += 2 * ship->dockingTime;
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


	auto waitForOGV = ogvWaitingTime(cargo, barge->localTimer);

	unloader.lock(barge->localTimer, waitForOGV + unloadingDuration + 2 * barge->dockingTime);

	barge->rememberState(Ship::State::WAITING, "no OGV on unloader");
	barge->localTimer += waitForOGV; 

	barge->rememberState(Ship::State::UNLOADING, "collected " + to_string_with_precision(ammount + cargo, 0));
	if (!useTows)
		barge->localTimer += 2 * barge->dockingTime; 
	barge->localTimer += unloadingDuration; 

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
	Tow::Ptr minTimestampedTow =  pool.begin()->first ;
	for (auto tow : pool)
		if (tow.first->localTimer < minTimestampedTow->localTimer)
			minTimestampedTow = tow.first;

	Ship::State &currentPos = pool.at(minTimestampedTow);
	if (currentPos == posA) currentPos = posB;
	else if (currentPos == posB) currentPos = posA;
	else throw std::logic_error("Invalid position!");

	minTimestampedTow->move(dist);
	minTimestampedTow->rememberState(currentPos, "Moving to waiting barge");
	return minTimestampedTow;
}

void Simulation::goUnloading(IBarge::Ptr barge)
{
	const float distUnloadToRief = totalDist - distLoadToRief;

	if (useTows)
	{
		Tow::Ptr freeRiverTow = getFreeRiverTow(Ship::State::GOING_UNLOAD);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeRiverTow->id);

		freeRiverTow->towBarge(barge);
		freeRiverTow->rememberState(Ship::State::GOING_UNLOAD, "draft " + to_string_with_precision(barge->draft(), 3));
		
		float distRiefToDrop = distLoadToDrop - distLoadToRief;
		if (distRiefToDrop < 0)
			throw std::runtime_error("Can't drop before rief");
		passRief(freeRiverTow, distLoadToRief, distRiefToDrop);
		freeRiverTow->dropBarges();
		riverTows[freeRiverTow] = Ship::State::WAITING;

		float distDropToUnloader = totalDist - distLoadToRief;
		Tow::Ptr freeSeaTow = getFreeSeaTow(Ship::State::WAITING);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeSeaTow->id);

		freeSeaTow->towBarge(barge);

		freeSeaTow->move(distDropToUnloader);

		barge->rememberState(Ship::State::WAITING, "unloader busy");
		const auto waitingTime = unloader.timeToUnlock(freeSeaTow->localTimer);
		freeSeaTow->localTimer += waitingTime;
		if (waitingTime)
		{
			barge->rememberState(Ship::State::DOCKING, "dancing with barges");
			freeSeaTow->localTimer += 2 * barge->dockingTime;
		}
		freeSeaTow->dropBarges();

		seaTows[freeSeaTow] = Ship::State::GOING_LOAD;
	}
	else
	{
		passRief(barge, distLoadToRief, distUnloadToRief);
		dockBTC(barge);
		barge->rememberState(Ship::State::WAITING, "unloader busy");
		barge->localTimer += unloader.timeToUnlock(barge->localTimer); // waiting for free unloader

	}

	barge->rememberState(Ship::State::UNLOADING);
}

Tow::Ptr Simulation::getFreeRiverTow(Ship::State requiredState)
{
	if (requiredState != Ship::State::GOING_UNLOAD && requiredState != Ship::State::WAITING)
		throw std::logic_error("Invalid state");
	Tow::Ptr freeRiverTow = findFreeTow(riverTows, Ship::State::GOING_UNLOAD);
	if (!freeRiverTow)
		freeRiverTow = moveTows(riverTows, Ship::State::GOING_UNLOAD, Ship::State::WAITING, distLoadToDrop);
	return freeRiverTow;
}
Tow::Ptr Simulation::getFreeSeaTow(Ship::State requiredState)
{
	if (requiredState != Ship::State::GOING_LOAD && requiredState != Ship::State::WAITING)
		throw std::logic_error("Invalid state");

	float distDropToUnloader = totalDist - distLoadToRief;
	Tow::Ptr freeSeaTow = findFreeTow(seaTows, requiredState);
	if (!freeSeaTow)
		freeSeaTow = moveTows(seaTows, Ship::State::WAITING, Ship::State::GOING_LOAD, distDropToUnloader);
	return freeSeaTow;
}

void Simulation::goLoading(IBarge::Ptr barge)
{
	const float distUnloadToRief = totalDist - distLoadToRief;
	if (useTows)
	{
		Tow::Ptr freeSeaTow = getFreeSeaTow(Ship::State::GOING_LOAD);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeSeaTow->id);

		float distDropToUnloader = totalDist - distLoadToRief;
				freeSeaTow->towBarge(barge);
		freeSeaTow->move(distDropToUnloader);
		freeSeaTow->dropBarges();

		seaTows[freeSeaTow] = Ship::State::WAITING;

		Tow::Ptr freeRiverTow = getFreeRiverTow(Ship::State::WAITING);
		barge->rememberState(Ship::State::WAITING, "waiting for tow " + freeRiverTow->id);

		freeRiverTow->towBarge(barge);
		freeRiverTow->rememberState(Ship::State::GOING_LOAD, "draft " + to_string_with_precision(barge->draft(), 3));

		float distRiefToDrop = distLoadToDrop - distLoadToRief;
		if (distRiefToDrop < 0)
			throw std::runtime_error("Can't drop before rief");
		passRief(freeRiverTow, distLoadToRief, distRiefToDrop);

		barge->rememberState(Ship::State::WAITING, "loader busy");
		const auto waitingTime = loader.timeToUnlock(freeRiverTow->localTimer);
		freeRiverTow->localTimer += waitingTime;
		if (waitingTime)
		{
			barge->rememberState(Ship::State::DOCKING, "dancing with barges");
			freeRiverTow->localTimer += 2 * barge->dockingTime;
		}
		freeRiverTow->dropBarges();
		riverTows[freeRiverTow] = Ship::State::GOING_UNLOAD;

		
	}
	else
	{
		dockBTC(barge);
		passRief(barge, distUnloadToRief, distLoadToRief);
		barge->rememberState(Ship::State::WAITING, "loader busy");
		barge->localTimer += loader.timeToUnlock(barge->localTimer); // waiting for free unloader

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

void Simulation::dockBTC(IBarge::Ptr ship)
{
	ship->localTimer += ship->riefPreparingTime;
	if (ship->riefPreparingTime)
		ship->rememberState(Ship::State::DOCKING, "BTC assembling");
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

