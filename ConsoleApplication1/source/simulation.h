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
	Timestamp ogvPeriod = 6 * 24 * 60;
	Timestamp ogvPeriodStddev = 1 / 3 * 24 * 60;
	Timestamp ogvChangingTime = 45;

	float run(Timestamp duration);

	void addContext(const ShipContext &context) { ships.emplace_back(context); }
	void setTides(TideTable::ConstPtr tideTable) { riefTide = tideTable; }


	void addTow(Tow::Ptr tow, bool seaOne) { (seaOne ? seaTows : riverTows)[tow] = tow->getState(); }

	Timestamp grepTows(bool getSeaTows, std::string str) const
	{
		Timestamp ans = 0;
		for (const auto tow : (getSeaTows ? seaTows : riverTows) )
			ans += tow.first->grepHistory(str);
		return ans;
	}

	template <typename T> void printHistory(T& stream)
	{
		unloader.printHistory(stream, "UNLOADER");
		loader.printHistory(stream, "LOADER");

		for (const auto pool : { seaTows, riverTows } )
			for (const auto tow : pool)
			{
				tow.first->printHistory(stream);
				stream << " MOVING time: "<< tow.first->grepHistory("MOVING") / 60 << " h\n";
				stream << " WAITING time: " << tow.first->grepHistory("WAITING") / 60 << " h\n";
			}
	}

	float distLoadToDrop{ 6.00f };


private:
	void load(IBarge::Ptr ship, const LoadingOrder& order);
	void goUnloading(IBarge::Ptr barge);
	void goLoading(IBarge::Ptr barge);

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

	bool useTows = true;

	UsedResource loader;
	UsedResource unloader;

	std::map<Tow::Ptr, Ship::State> seaTows;
	std::map<Tow::Ptr, Ship::State> riverTows;

	Tow::Ptr getFreeSeaTow(Ship::State requiredState);
	Tow::Ptr getFreeRiverTow(Ship::State requiredState);


	float totalDist{ 23.5f };
	float distLoadToRief{ 3.78f };

	TideTable::ConstPtr riefTide;

	float ammount{ 0.f };
};
