#pragma once



#include "core.h"



struct TideTable : public InterpolatedTable<Timestamp>
{
public:
	using Ptr = std::shared_ptr<TideTable>;
	using ConstPtr = std::shared_ptr<const TideTable>;

	Timestamp getPeriod() const
	{
		if (table.empty())
			throw std::range_error("Tide table empty");
		auto lastIterator = table.end();
		return (--lastIterator)->first;
	}

	Timestamp timeToPossibleDraft(Timestamp currentTime, float minDraft) const
	{
		const auto period = getPeriod();
		const Timestamp step = 1; // period / table.size() / 2;
		if (!step)
			throw std::runtime_error("Zero step?!");
		Timestamp waitingTime = 0;
		while (draft(currentTime + waitingTime) < minDraft && waitingTime < period)
			waitingTime += step;
		if (waitingTime >= period)
			throw std::runtime_error("Impossible to pass - to little minDraft = "+ std::to_string(minDraft));
		return waitingTime;
	}

	float draft(Timestamp time) const { return get(time % getPeriod()); }
};


struct TideData
{

	bool readFromFile(const std::string& path)
	{
		data.clear();

		std::ifstream infile(path);
		std::string line;
		if (!infile.is_open())
		{
			std::cerr << "Unabe to read file " << path << " - make sure the file exists" << std::endl;
			return false;
		}
		int prevDate = 0;
		while (std::getline(infile, line))
		{
			std::istringstream iss(line);
			int date;


			if (!line.size())
				continue;
			bool dataTaken = bool(iss >> date);
			if (!dataTaken || date != prevDate + 1)
				std::cerr << "Unable to parse string: \"" << line << "\" - string must begin with a date (expected: " << prevDate + 1 << ")" << std::endl;
			else
			{
				std::vector<float> hoursdata(24);
				bool correct = true;
				for (int hour = 0; correct && hour < 24; ++hour)
					correct = bool(iss >> hoursdata[hour]);
				if (correct)
				{
					for (auto hourValue : hoursdata)
						data.push_back(hourValue);
					++prevDate;
				}
				else
					std::cerr << "Unable to parse string: \"" << line << "\" - line must have 25 values " << std::endl;

			}
		}
		return true;
	}

	TideTable::Ptr toTideTable(float multipler = 60, float bonus = 0.f)
	{
		TideTable::Ptr table = std::make_shared<TideTable>();
		for (auto counter = 0; counter < data.size(); ++counter)
			table->add(static_cast<Timestamp>(counter * multipler), data.at(counter) + bonus);
		return table;
	}

private:
	std::vector<float> data;
};