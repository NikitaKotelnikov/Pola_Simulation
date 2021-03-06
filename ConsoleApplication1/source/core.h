#pragma once

#include <vector>
#include <map>
#include <queue>
#include <iostream>
#include <fstream>
#include <memory>
#include <iomanip>

using Timestamp = unsigned long long; // minutes!




template <typename T>
std::string to_string_with_precision(const T a_value, const int n = 6)
{
	std::ostringstream out;
	out << std::setprecision(n) << a_value;
	return out.str();
}

inline std::string printTime(Timestamp ts)
{
	const auto week = ts / 60 / 24 / 7;
	const auto dayOfWeek = (ts / 60 / 24 - week * 7);
	const auto hour = ts % (60 * 24) / 60;
	const auto minute = ts % 60;
	const auto print00 = [](Timestamp t) {return t > 9 ? std::to_string(t) : "0" + std::to_string(t); };
	return "w" + std::to_string(1 + week) + ", d" + std::to_string(1 + dayOfWeek) + ", " + print00(hour) + ":" + print00(minute);
}

inline std::string printTimeShort(Timestamp ts)
{
  const auto days = std::floor(static_cast<float>(ts) / 60 / 24);
  const auto hour = ts % (60 * 24) / 60;
  const auto minute = ts % 60;
  const auto print00 = [](Timestamp t) {return t > 9 ? std::to_string(t) : "0" + std::to_string(t); };
  return (days ? std::to_string(days) + " day(s), " : std::string() ) + print00(hour) + ":" + print00(minute);
}

template <typename KeyType>
struct InterpolatedTable
{
	using Ptr = std::shared_ptr<InterpolatedTable<KeyType>>;
	using ConstPtr = std::shared_ptr<const InterpolatedTable<KeyType>>;

	void add(KeyType t, float val) { table.emplace(t, val); }
	bool readFromFile(const std::string& path, float keyMultipler = 1.f, float valueMultipler = 1.f, float keyAdd = 0.f, float valueAdd = 0.f)
	{
		std::ifstream infile(path);
		std::string line;
		if (!infile.is_open())
		{
			std::cerr << "Unabe to read file " << path << " - make sure the file exists" << std::endl;
			return false;
		}
		while (std::getline(infile, line))
		{
			std::istringstream iss(line);
			float value;
			KeyType key;
			if (iss >> key >> value)
				add(key * keyMultipler + keyAdd, value * valueMultipler + valueAdd);
			else if (line.size())
				std::cerr << "Unable to parse string: \"" << line << "\" - please use strings like \"[key] [value]\"" << std::endl;
		}
		return true;
	}

	float get(KeyType key) const
	{
		if (table.empty())
			throw std::range_error("Table empty");

		if (table.count(key))
			return table.at(key);

		auto upper = table.upper_bound(key);
		if (upper == table.end() || upper == table.begin())
			throw std::range_error("Something went wrong :(");

		auto lower = upper; 
		--lower;
		return interpolate(*lower, *upper, key);
	}

protected:
	std::map<KeyType, float> table;

	static float interpolate(const std::pair<KeyType, float>& lower, const std::pair<KeyType, float>& upper, KeyType key)
	{
		if (key > std::max(lower.first, upper.first) || key < std::min(lower.first, upper.first))
			throw std::range_error("Not in bounds! Low = "+std::to_string(lower.first) + ", upper = " + std::to_string(upper.first) + ", asked = " + std::to_string(key));

		const auto diff = [](KeyType t1, KeyType t2) { return static_cast<float>(t1) - static_cast<float>(t2); };
		const float k = (upper.second - lower.second) / diff(upper.first, lower.first);
		return lower.second + k * diff(key, lower.first);
	}
};

using ShipDraftTable = InterpolatedTable<float>;




struct UsedResource
{
	using Ptr = std::shared_ptr<UsedResource>;
	using ConstPtr = std::shared_ptr<const UsedResource>;

	Timestamp lock(Timestamp currentTime, Timestamp duration, std::string id)
	{
		const auto waitingTime = timeToUnlock(currentTime, id);
		history.emplace(currentTime + waitingTime, duration);
		if (!id.empty())
			events[currentTime + waitingTime] = id;
		return waitingTime;
	}
	void clear() { history.clear(); events.clear(); }

	std::string id{ "Unnamed_resource" };
  template <typename T>
  void printHistory(T& stream) const
  {
    stream << "\nLog of \"" << id << "\":\n";
	for (auto &lockEvent : history)
	{
		stream << "locked: " << printTime(lockEvent.first) << "\t - " << printTime(lockEvent.first + lockEvent.second) << "\t(used for " << printTimeShort(lockEvent.second) << ")";
		if (events.count(lockEvent.first))
			stream << "\t" << events.at(lockEvent.first);
		stream << "\n";
	}
  }

  bool isFreeAfter(Timestamp currentTime) const
  {
	  return history.upper_bound(currentTime) == history.end();
  }

	Timestamp timeToUnlock(Timestamp currentTime, std::string id) const
	{
		if (history.empty())
			return 0;
		auto lastLock = history.lower_bound(currentTime);
		if (lastLock == history.begin()) 
			return 0;
		--lastLock;

		const auto lockStart = lastLock->first;
		const auto lockEnd = lastLock->first + lastLock->second;
		//std::cerr << "lockStart: " << lockStart << ", lockEnd: " << lockEnd << std::endl;
		if (currentTime >= lockStart && currentTime <= lockEnd ) // && (!id.empty() && events.count(lockStart) && events.at(lockStart) != id)
			return lockEnd - currentTime;
		return 0;
	}

private:
	std::map<Timestamp, Timestamp> history;
	std::map<Timestamp, std::string> events;
};

