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

	float loading = 0;
	float unloading = 0;
	float lostWaitingOGV = 0;

	float cargo = 0;
	float btcCargo = 0;
	int passes = 0;
};

int main()
{

	std::ofstream outFile;
	outFile.open("out.txt");
	outFile
		<< "Cargo(tonns)\t"
		<< "BTC cargo(tonns)\t"
		<< "LoadingRate(tonns/hour)\t"
		<< "Ships\t" << "BTCs\t"
		<< "Ammount(tonns)\t"
		<< "Rides\t"
		<< "LostOnTide(hours)\t"
		<< "LostOnLoading(hours)\t"
		<< "LostOnUnloading(hours)"
		<< "LostWaitingOGV(hours)"
		<< "\n";

	try
	{
		TideData tideData;
		tideData.readFromFile("tides_data.txt");

		//TideTable::Ptr tides = std::make_shared<TideTable>();
		//tides->readFromFile("tide.txt", 60, 1, 0, 1.4);

		TideTable::Ptr tides = tideData.toTideTable(60.f, 1.4);

		ShipDraftTable::Ptr drafts = std::make_shared<ShipDraftTable>();
		drafts->readFromFile("draft.txt");



		std::map<int, std::map<float, std::pair<float, SimulationStats>> > bests;
		//float cargo = 5200;
		//float intensity = 2000 / 60;

		std::cerr << " Simulating..." << std::endl;


		float simulationDays = 30;
		Timestamp simulationTime = simulationDays * 24 * 60;
		for (int btcsNum : {0, 1, 2})
			for (int shipsNum : { 0, 1, 2})
				if (shipsNum + btcsNum > 0 && shipsNum + btcsNum <= 3)
				{

					auto prepareShipContexts = [&drafts](int shipsNum, float cargo, float intensity)
					{
						std::vector<ShipContext> contexts(shipsNum);
						int counter = 0;
						for (auto &context : contexts)
						{
							auto barge = std::make_shared<Barge>();
							barge->setDraftTable(drafts);
							context.ship = barge;
							context.ship->id = "Ship #" + std::to_string(counter + 1);
							context.orders = std::queue<LoadingOrder>();
							for (int i = 0; i < 1000; ++i)
								context.orders.emplace(cargo, intensity);

							++counter;
						}
						return contexts;
					};

					auto prepareBTCContexts = [](int btcsNum, float cargo, float intensity)
					{
						std::vector<ShipContext> contexts(btcsNum);
						int counter = 0;
						for (auto &context : contexts)
						{
							auto btc = std::make_shared<BTC>();
							context.ship = btc;
							context.ship->id = "BTC #" + std::to_string(counter + 1);

							context.orders = std::queue<LoadingOrder>();
							for (int i = 0; i < 1000; ++i)
								context.orders.emplace(cargo, intensity);

							++counter;
						}
						return contexts;
					};

					for (float intensity : {1000 / 60.f, 1500 / 60.f, 2000 / 60.f})
					{

						std::pair<float, SimulationStats> &localbest = bests[shipsNum][intensity];
						localbest.first = -1;
						for (float btcCargo = 7000.f; btcCargo <= 12000.f; btcCargo += 200)
							for (float cargo = 3700.f; cargo <= 7200; cargo += 50.f)
							{

								std::vector<ShipContext> contexts;
								for (auto shipContext : prepareShipContexts(shipsNum, cargo, intensity))
									contexts.push_back(shipContext);
								for (auto btcContext : prepareBTCContexts(btcsNum, btcCargo, intensity))
									contexts.push_back(btcContext);

								Simulation simulation;
								for (auto &context : contexts)
									simulation.addContext(context);
								simulation.setTides(tides);
								float ammount = simulation.run(simulationTime);

								SimulationStats stats;

								//std::cerr << "      result for cargo = " << cargo << ", intensity = " << intensity * 60 << " : " << ammount << ", ships usage:";
								for (const auto &context : contexts)
								{
									stats.lostWaitingTide += context.ship->grepHistory("low tide") / 60;
									stats.lostWaitingLoader += context.ship->grepHistory("loader busy") / 60;
									stats.lostWaitingUnloader += context.ship->grepHistory("unloader busy") / 60;
									stats.loading += context.ship->grepHistory("LOADING") / 60;
									stats.unloading += context.ship->grepHistory("UNLOADING") / 60;
									stats.lostWaitingOGV += context.ship->grepHistory("no OGV") / 60;



									stats.passes += context.ship->loadsCounter;
									stats.cargo = cargo;
									stats.btcCargo = btcCargo;
									/*	std::cerr << "\n\t" << context.ship->id << ": " << context.ship->loadsCounter << " passes, "
											<< " waiting time: " << context.ship->grepHistory("low tide") << "h for draft, "
											<< context.ship->grepHistory("busy") / 60 << "h for (un)loader";*/
								}





								if (ammount > localbest.first)
									localbest = std::make_pair(ammount, stats);
							}
						const auto &bestStats = localbest.second;
						outFile
							<< bestStats.cargo << "\t"
							<< bestStats.btcCargo << "\t"
							<< intensity * 60 << "\t"
							<< shipsNum << "\t"
							<< btcsNum << "\t"
							<< localbest.first << "\t"
							<< bestStats.passes << "\t"
							<< bestStats.lostWaitingTide << "\t"
							<< bestStats.lostWaitingLoader << "\t"
							<< bestStats.lostWaitingUnloader << "\t"
							<< bestStats.lostWaitingOGV << "\t"
							<< "\n";


						std::vector<ShipContext> contexts;
						for (auto shipContext : prepareShipContexts(shipsNum, bestStats.cargo, intensity))
							contexts.push_back(shipContext);
						for (auto btcContext : prepareBTCContexts(btcsNum, bestStats.btcCargo, intensity))
							contexts.push_back(btcContext);

						std::ostringstream filename;
						filename << "Log_" << shipsNum << "ships_" << btcsNum << "BTCs_" << intensity * 60 << "tph_" << bestStats.cargo << ".txt";

						Simulation simulation;
						for (auto &context : contexts)
							simulation.addContext(context);
						simulation.setTides(tides);
						float ammount = simulation.run(simulationTime);

						std::ofstream outFile;

						outFile.open("simulation_output/" + filename.str());
						outFile << "Log of simulation for " << shipsNum << " ships, "
							<< btcsNum << " BTCS, loading rate = " << intensity * 60 << " tonns/h, "
							<< "cargo = " << bestStats.cargo << " tonns, BTC cargo = " << bestStats.btcCargo << " tonns\n";

						for (auto &context : contexts)
							context.ship->printHistory(outFile);
						for (auto &ogv : simulation.oceanQueue)
							ogv->printHistory(outFile);
						simulation.loader.printHistory(outFile, "LOADER");
						simulation.unloader.printHistory(outFile, "UNLOADER");

						outFile.close();
					}
				}
		//std::cerr << "Best result on cargo = " << best.first << " : summary " << best.second / simulationDays * 30.5 << " tonns/month" << std::endl;
		outFile.close();

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