#include "stdafx.h"

#include "ship.h"



void Ship::rememberState(State askedState, std::string comment)
{
	history[localTimer] = printState(askedState) + "\t" + comment;
	state = askedState;
}


Timestamp Ship::grepHistory(const std::string &str)
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


void Ship::load(float cargoToLoad)
{
	if (cargoToLoad + cargo > capacity)
		throw std::runtime_error("Ordered to load more than possible: asked " + std::to_string(cargoToLoad) + ", already " + std::to_string(cargo) + " / " + std::to_string(capacity));
	cargo += cargoToLoad;
}
float Ship::unload()
{
	const float cargoToUnload = cargo;
	cargo = 0;
	++loadsCounter;
	return cargoToUnload;
}


void Ship::clear()
{
	localTimer = 0;
	cargo = 0.f;
	loadsCounter = 0;
	state = State::LOADING;
	history.clear();
}

std::string Ship::printState(State st)
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

