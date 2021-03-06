#pragma once

#include "core.h"
#include "ship.h"
#include "tide.h"
#include <random>
#include <queue>

struct TugPositions
{
	std::map<Tow::Ptr, Ship::State> positions;
	Ship::State posA, posB;
	float dist;
};

struct Simulation
{
	bool useOGV = true;
	 std::vector<OceanVeichle::Ptr> oceanQueue;
	Timestamp ogvPeriod = 6 * 24 * 60;
	Timestamp ogvPeriodStddev = 24 * 60 ;
	Timestamp ogvChangingTime = 45;
  float ogvReserve = 5000;

	float run(Timestamp duration);

	void addContext(const ShipContext &context) { ships.emplace_back(context); }
	void setTides(TideTable::ConstPtr tideTable) { riefTide = tideTable; }


	void addTow(Tow::Ptr tow, bool river) { (river ? riverTows : seaTows).positions[tow] = tow->getState(); }

	Timestamp grepTows(std::string str, bool river) const
	{
		Timestamp ans = 0;
		for (const auto tow : (river ? riverTows : seaTows).positions)
			ans += tow.first->grepHistory(str);
		return ans;
	}

	template <typename T> void printHistory(T& stream)
	{
		for (auto unloader : unloaders)
		   unloader->printHistory(stream);
		loader->printHistory(stream);

    for (const auto & pool : { riverTows, seaTows })
			for (const auto tow : pool.positions)
			{
				tow.first->printHistory(stream);
				stream << " MOVING time: "<< tow.first->grepHistory("MOVING") / 60 << " h\n";
				stream << " WAITING time: " << tow.first->grepHistory("WAITING") / 60 << " h\n";
			}
	}

	float distLoadToDrop{ 6.00f };

  bool useRiverTows = true;

private:
	void load(IBarge::Ptr ship, const LoadingOrder& order);
	void goUnloading(IBarge::Ptr barge);
	void goLoading(IBarge::Ptr barge);

	void unload(IBarge::Ptr ship);
	void passRief(Ship::Ptr ship, float toRief, float fromRief);

	void initOGVs(Timestamp duration);
	OceanVeichle::Ptr getClosestOgv();
	Timestamp ogvWaitingTime(float cargo, Timestamp time);

  void waitForResource(UsedResource::Ptr usedResource, Ship::Ptr ship);
  std::map<IBarge::Ptr, UsedResource::Ptr> blockers;

	ShipContext &getNextShip();
	Timestamp process(ShipContext& context);

	float unloadingSpeed{ 759.f / 60 };

	std::vector<ShipContext> ships;

	std::map<Ship::State, std::vector<IBarge::Ptr>> waitingBarges;

//	bool useTows = true;

	UsedResource::Ptr loader {new UsedResource };
	std::vector<UsedResource::Ptr> unloaders;
	std::map<UsedResource::ConstPtr, OceanVeichle::ConstPtr> unloaderToOGV;
	bool ballanceUnloaders(Timestamp currentTime);

	UsedResource::Ptr getUnloader(Timestamp currentTime) const;

	//std::map<Tow::Ptr, Ship::State> seaTows;


	TugPositions riverTows;
  TugPositions seaTows;

	//Tow::Ptr getFreeSeaTow(Ship::State requiredState);
	Tow::Ptr getFreeTug(TugPositions& pool, Ship::State requiredState);
	Tow::Ptr summonTow(TugPositions& pool, Ship::State requiredState, IBarge::Ptr barge);



	float totalDist{ 23.5f };
	float distLoadToRief{ 3.78f };

	TideTable::ConstPtr riefTide;

	float ammount{ 0.f };
};
