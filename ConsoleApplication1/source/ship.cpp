#include "stdafx.h"

#include "ship.h"



void Ship::rememberState(State askedState, std::string comment, bool rewrite)
{
  if (rewrite)
    history.erase(localTimer);
  history.emplace(localTimer, printState(askedState) + "\t" + comment);
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
		if (++eventIterator == history.end()) break;

		const auto next = (eventIterator)->first;
		if (contains && next > curr)
			sum += next - curr;
		if (next < curr)
			throw std::runtime_error("Damn, history records not ordered");
	} while (eventIterator != history.end());
	return sum;
}


void Ship::load(float cargoToLoad)
{
	if (cargoToLoad < 0)
		throw std::runtime_error("Asked to load negative cargo");

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

Timestamp Ship::move(float dist)
{
	if (dist < 0)
		throw std::runtime_error("Asked to move on negative distance");

	Timestamp movingTime = static_cast<Timestamp>(dist / velocity());
  rememberState(State::MOVING, "Moving on dist " + to_string_with_precision(dist, 3) +", velocity " + to_string_with_precision(velocity() * 60, 2));

	localTimer += movingTime;
	return movingTime;
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
  case State::MOVING:       return "MOVING";

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

std::vector<IBarge::Ptr> Tow::dropBarges(Timestamp operationTime)
{
	for (auto barge : barges)
	{
		barge->rememberState(Ship::State::DOCKING, "Being towed by " + id);
		synchroTime(barge);
	}

	Timestamp spentTime = 0;
	for (auto barge : barges)
	{
		localTimer += operationTime;
		spentTime += operationTime;
		rememberState(Ship::State::DOCKING, "Dropping " + barge->id);
		barge->localTimer = localTimer;
		barge->rememberState(Ship::State::DOCKING, "Droped by " + id);
	}
	std::vector<IBarge::Ptr> freeBarges = barges;
	barges.clear();
  rememberState(Ship::State::WAITING, "Waiting for another barge(s)");

	return freeBarges;
}

void Tow::towBarge(IBarge::Ptr barge, Timestamp operationTime)
{
	if (bargesTowed() < maxBarges)
		barges.push_back(barge);
	else 
		throw std::runtime_error("Too many barges towed!");

	//rememberState(Ship::State::WAITING, "Having a rest (nothing to tow?)");
	synchroTime(barge);
	localTimer += operationTime;
	rememberState(Ship::State::DOCKING, "Towing " + barge->id);
	barge->rememberState(Ship::State::DOCKING, "Connecting to " + id);
	barge->localTimer += operationTime;

	barge->rememberState(Ship::State::DOCKING, "Towed by " + id);
}