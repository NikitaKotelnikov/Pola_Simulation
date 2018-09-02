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

float DraftingOne::draftByTable(float cargo) const 
{
	if (!draftTable) 
		throw std::runtime_error("No draft table!"); 
	return draftTable->get(cargo);
}


void Tow::load(float cargoToLoad)
{
	if (barges.empty())
		throw std::logic_error("No barges towed!");
	for (auto barge : barges)
	{
		float loatToThis = std::max(cargoToLoad, barge->capacity);
		barge->load(loatToThis);
		cargoToLoad -= loatToThis;
	}
}
float Tow::unload()
{
	float unloaded = 0;
	if (barges.empty())
		throw std::logic_error("No barges towed!");
	for (auto barge : barges)
		unloaded += barge->unload();
	return unloaded;
}
float Tow::draft() const
{
	float maxDraft = ballastDraft;
	for (auto barge : barges)
		maxDraft = std::max(maxDraft, barge->draft());
	return maxDraft;
}

int Tow::dropBarges(std::vector<IBarge::Ptr> &freeBarges)
{
	freeBarges = barges;
	barges.clear();
	return static_cast<int>(freeBarges.size());
}

void Tow::towBarge(IBarge::Ptr barge)
{
	if (bargesTowed() < maxBarges)
		barges.push_back(barge);
	else 
		throw std::runtime_error("Too many barges towed!");
}