#pragma once

#include "core.h"
#include "ship.h"
#include "tide.h"
#include <random>

struct ShipContext
{
	IBarge::Ptr ship{ nullptr };
	std::queue<LoadingOrder> orders;
};


struct Simulation
{
	bool useOGV = true;
	std::vector<OceanVeichle::Ptr> oceanQueue;
	Timestamp ogvPeriod = 1 * 24 * 60;
	Timestamp ogvPeriodStddev = 1 / 3 * 24 * 60;
	Timestamp ogvChangingTime = 30;

	float run(Timestamp duration);

	void addContext(const ShipContext &context) { ships.emplace_back(context); }
	void setTides(TideTable::ConstPtr tideTable) { riefTide = tideTable; }

	UsedResource loader;
	UsedResource unloader;

	bool addTow(Tow::Ptr tow, bool seaOne) { (seaOne ? seaTows : riverTows)[tow] = Ship::State::WAITING; }

private:
	void load(IBarge::Ptr ship, const LoadingOrder& order);
	void goUnloading(IBarge::Ptr barge);

	void unload(IBarge::Ptr ship);
	void passRief(Ship::Ptr ship, float toRief, float fromRief);
	void dockBTC(IBarge::Ptr ship);

	void initOGVs(Timestamp duration);
	OceanVeichle::Ptr getClosestOgv();
	Timestamp ogvWaitingTime(float cargo, Timestamp time);


	ShipContext &getNextShip();
	Timestamp process(ShipContext& context);

	float unloadingSpeed{ 759.f / 60 };

	std::vector<ShipContext> ships;

	std::map<Ship::State, std::vector<IBarge::Ptr>> waitingBarges;

	bool useTows = false;

	std::map<Tow::Ptr, Ship::State> seaTows;
	std::map<Tow::Ptr, Ship::State> riverTows;


	float totalDist{ 23.5f };
	float distLoadToRief{ 3.78f };
	float distLoadToDrop{ 6.00f };

	TideTable::ConstPtr riefTide;

	float ammount{ 0.f };
};
