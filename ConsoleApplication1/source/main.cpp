#include "stdafx.h"

#include <vector>
#include <map>
#include <queue>
#include <iostream>
#include <fstream>
#include <memory>

#include "core.h"
#include "ship.h"
#include "simulation.h"

struct SimulationStats
{
	float lostWaitingTide = 0;
	float lostWaitingLoader = 0;
	float lostWaitingUnloader = 0;
	float lostWaitingRiverTow = 0;
	float lostWaitingSeaTow = 0;

	float spentByRiverTowsOnMoving = 0;
	float spentByRiverTowsOnWaiting = 0;
	float spentBySeaTowsOnMoving = 0;
	float spentBySeaTowsOnWaiting = 0;

	float loading = 0;
	float unloading = 0;
	float lostWaitingOGV = 0;

	float optimalDistLoadToDrop = -1;

	float bargeCargo = 0;
	float RSDcargo = 0;
	int passes = 0;
};

int main()
{

	std::ofstream outFile;
	outFile.open("out.txt");
	outFile
		<< "Barge Cargo (tonns)\t"
		<< "RSD cargo (tonns)\t"
		<< "Loading Rate (tonns/hour)\t"
		<< "Barges\t"
		<< "RSDs\t"
		<< "Ammount (tonns)\t"
		<< "Rides\t"
		<< "Lost Waiting Tide (hours)\t"
		<< "Lost Waiting Loadier (hours)\t"
		<< "Lost Waiting Unloader (hours)\t"
		<< "Lost Waiting OGV (hours)\t"
		<< "Tugs\t"
	//	<< "Sea Tows\t"
		<< "Lost Waiting Tug (hours)\t"
		//<< "Lost Waiting Sea Tow (hours)\t"
		<< "Spent By Tugs On Moving (hours)\t"
		<< "Spent By Tugs On Waiting (hours)\t"
	//	<< "Spent By Sea Tows On Moving (hours)\t"
	//	<< "Spent By Sea Tows On Waiting (hours)\t"
	//	<< "Optimal Dist Load->Drop (nm)\t"
		<< "\n";
	outFile.close();

	try
	{
		TideData tideData;
		tideData.readFromFile("tides_data.txt");
		TideTable::Ptr tides = tideData.toTideTable(60.f, 1.4);

		ShipDraftTable::Ptr drafts = std::make_shared<ShipDraftTable>();
		drafts->readFromFile("draft.txt");



		std::map<int, std::map<float, std::pair<float, SimulationStats>> > bests;

		std::cerr << " Simulating..." << std::endl;

		auto addTows = [](Simulation& simulation, int tows)
		{
			for (int r = 0; r < tows; ++r)
			{
				Tow::Ptr riverTow = std::make_shared<Tow>();
				riverTow->id = "TUG #" + std::to_string(r + 1);
				riverTow->towingVelocity = 4.f / 60;
				riverTow->movingVelocity = 5.f / 60;
				riverTow->rememberState(Ship::State::WAITING, "Teleport");
				simulation.addTow(riverTow);
			}
			
		};

		std::vector<std::pair<int, int>> riverSeaTows;
		riverSeaTows.push_back(std::make_pair(1, 0));


		float simulationDays = 30;
		Timestamp simulationTime = simulationDays * 24 * 60;
		for (int rsdNum : {1})
			for (int bargesNum : {1, 2})
				for (auto towsOnRiverAnsSea : riverSeaTows)
					//if (shipsNum + btcsNum > 0 && shipsNum + btcsNum <= 3)
					{

						auto prepareShipContexts = [bargesNum](int shipsNum, float cargo, float intensity)
						{
							std::vector<ShipContext> contexts(shipsNum);
							int counter = 0;
							for (auto &context : contexts)
							{
								auto barge = std::make_shared<Washtub>(bargesNum == 1 ? 2 : 1);
								//barge->setDraftTable(drafts);
								context.ship = barge;
								context.ship->id = "BARGE #" + std::to_string(counter + 1);
								context.orders = std::queue<LoadingOrder>();
								for (int i = 0; i < 1000; ++i)
									context.orders.emplace(cargo, intensity);

								++counter;
							}
							return contexts;
						};

						auto prepareRSDContexts = [&drafts](int rsdNum, float cargo, float intensity)
						{
							std::vector<ShipContext> contexts(rsdNum);
							int counter = 0;
							for (auto &context : contexts)
							{
								auto rsd = std::make_shared<RSD>();
								context.ship = rsd;
                rsd->setDraftTable(drafts);
								context.ship->id = "RSD #" + std::to_string(counter + 1);

								context.orders = std::queue<LoadingOrder>();
								for (int i = 0; i < 1000; ++i)
									context.orders.emplace(cargo, intensity);

								++counter;
							}
							return contexts;
						};

						for (float intensity : {1500 / 60.f, 2000 / 60.f})
						{

							std::pair<float, SimulationStats> &localbest = bests[bargesNum][intensity];
							localbest.first = -1;
							//float btcCargo = 0;
							for (float RSDcargo = 3000.f; RSDcargo <= 7200.f; RSDcargo += 200)
							for (float bargeCargo = 4000.f; bargeCargo <= (bargesNum == 1 ? 12000 : 6000); bargeCargo += 250.f)
								//for (float distance = 4.0f; distance < 16.f; distance += 1)
							{
								float distance = -1.0f;

								std::vector<ShipContext> contexts;
								for (auto shipContext : prepareShipContexts(bargesNum, bargeCargo, intensity))
									contexts.push_back(shipContext);
								for (auto btcContext : prepareRSDContexts(rsdNum, RSDcargo, intensity))
									contexts.push_back(btcContext);

								Simulation simulation;
								for (auto &context : contexts)
									simulation.addContext(context);
								simulation.setTides(tides);
								addTows(simulation, towsOnRiverAnsSea.first);
								simulation.distLoadToDrop = distance;

								float ammount = simulation.run(simulationTime);

								SimulationStats stats;

								//std::cerr << "      result for cargo = " << cargo << ", intensity = " << intensity * 60 << " : " << ammount << ", ships usage:";
								for (const auto &context : contexts)
								{
								 	stats.lostWaitingTide += context.ship->grepHistory("low tide") / 60 / contexts.size();
									stats.lostWaitingLoader += context.ship->grepHistory("loader busy") / 60 / contexts.size();
									stats.lostWaitingUnloader += context.ship->grepHistory("unloader busy") / 60 / contexts.size();
									stats.loading += context.ship->grepHistory("LOADING") / 60 / contexts.size();
									stats.unloading += context.ship->grepHistory("UNLOADING") / 60 / contexts.size();
									stats.lostWaitingOGV += context.ship->grepHistory("no OGV") / 60 / contexts.size();
									stats.lostWaitingRiverTow += context.ship->grepHistory("waiting for tow") / 60 / contexts.size();
									stats.lostWaitingSeaTow += context.ship->grepHistory("waiting for tow SEA") / 60 / contexts.size();


									stats.passes += context.ship->loadsCounter / contexts.size();
								}

								stats.spentByRiverTowsOnMoving = simulation.grepTows("Moving on dist") / 60 / towsOnRiverAnsSea.first;
								stats.spentByRiverTowsOnWaiting = simulation.grepTows("Waiting for another barge") / 60 / towsOnRiverAnsSea.first;
								stats.lostWaitingTide += simulation.grepTows("low tide") / 60 / towsOnRiverAnsSea.first;

								stats.bargeCargo = bargeCargo;
								stats.RSDcargo = RSDcargo;

								stats.optimalDistLoadToDrop = distance;


								if (ammount > localbest.first)
									localbest = std::make_pair(ammount, stats);
							}
							const auto &bestStats = localbest.second;
						//	if (localbest.first < 500000)
							//	continue;

							outFile.open("out.txt", std::ios_base::app);
							outFile
								<< bestStats.bargeCargo << "\t"
								<< bestStats.RSDcargo << "\t"
								<< intensity * 60 << "\t"
								<< bargesNum << "\t"
								<< rsdNum << "\t"
								<< localbest.first << "\t"
								<< bestStats.passes << "\t"
								<< bestStats.lostWaitingTide << "\t"
								<< bestStats.lostWaitingLoader << "\t"
								<< bestStats.lostWaitingUnloader << "\t"
								<< bestStats.lostWaitingOGV << "\t"
								<< towsOnRiverAnsSea.first << "\t"
							//	<< towsOnRiverAnsSea.second << "\t"
								<< bestStats.lostWaitingRiverTow << "\t"
							//	<< bestStats.lostWaitingSeaTow << "\t"
								<< bestStats.spentByRiverTowsOnMoving << "\t"
								<< bestStats.spentByRiverTowsOnWaiting << "\t"
							//	<< bestStats.spentBySeaTowsOnMoving << "\t"
							//	<< bestStats.spentBySeaTowsOnWaiting << "\t"
							//	<< bestStats.optimalDistLoadToDrop << "\t"
								<< "\n";
							outFile.close();


							std::vector<ShipContext> contexts;
							for (auto shipContext : prepareShipContexts(bargesNum, bestStats.bargeCargo, intensity))
								contexts.push_back(shipContext);
							for (auto rsdContext : prepareRSDContexts(rsdNum, bestStats.RSDcargo, intensity))
								contexts.push_back(rsdContext);

							std::ostringstream filename;
							filename << "Log_" << bargesNum << "ships_" << rsdNum << "BTCs_"
								<< intensity * 60 << "tph_" << bestStats.bargeCargo << ".txt";

							Simulation simulation;
							for (auto &context : contexts)
								simulation.addContext(context);
							simulation.setTides(tides);
							simulation.distLoadToDrop = bestStats.optimalDistLoadToDrop;
							addTows(simulation, 1);

							float ammount = simulation.run(simulationTime);

							std::ofstream logFile;

							logFile.open("simulation_output/" + filename.str());
							logFile << "Log of simulation for " << bargesNum << " barges, "
								<< rsdNum << " BTCs, loading rate = " << intensity * 60 << " tonns/h, "
								<< "cargo = " << bestStats.bargeCargo << " tonns, BTC cargo = " << bestStats.RSDcargo << " tonns\n";

							for (auto &context : contexts)
								context.ship->printHistory(logFile);
							for (auto &ogv : simulation.oceanQueue)
									ogv->printHistory(logFile);
							simulation.printHistory(logFile);

							logFile.close();
						}
					}
		//std::cerr << "Best result on cargo = " << best.first << " : summary " << best.second / simulationDays * 30.5 << " tonns/month" << std::endl;

		/**/

		// std::cerr << "result: " << rslt << std::endl;
	}
	catch (std::exception ex)
	{
		std::cerr << "Exception caught: " << ex.what() << std::endl;
	}
	std::cerr << "Done." << std::endl;

	system("pause");
	return 1;
}