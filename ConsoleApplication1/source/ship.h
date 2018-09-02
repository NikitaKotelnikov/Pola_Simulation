#pragma once

#include "core.h"

struct Ship
{
	enum class State { LOADING, UNLOADING, GOING_LOAD, GOING_UNLOAD, WAITING, RIEF_PASSED, DOCKING };
	using Ptr = std::shared_ptr<Ship>;
	using ConstPtr = std::shared_ptr<const Ship>;

	void rememberState(State askedState, std::string comment = "");

	template <typename T>
	void printHistory(T& stream) const
	{
		stream << "\nLog of \"" << id << "\":\n";
		for (auto &shipEvent : history)
			stream << printTime(shipEvent.first) << "\t" << shipEvent.second << "\n";
	}
	Timestamp grepHistory(const std::string &str);

	virtual void load(float cargoToLoad);
	virtual float unload();
	Timestamp move(float dist);
	
	virtual float draft() const { throw std::logic_error("Not implemented, stupido!"); };
	virtual float velocity() const { throw std::logic_error("Not implemented, stupido!"); }

	State getState() const { return state; }

	void clear();

protected:
	static std::string printState(State st);
	inline void synchroTime(Ship::Ptr ship)
	{
		auto synchroTime = std::max(ship->localTimer, localTimer);
		localTimer = synchroTime;
		ship->localTimer = synchroTime;
	}

protected:
	std::map < Timestamp, std::string > history;
	State state{ State::LOADING };

public:
	std::string id = "UNNAMED";
	Timestamp localTimer = 0;

	float capacity{ 0.f };
	float cargo{ 0.f };
	int loadsCounter = 0;
};



struct IBarge : Ship
{
	using Ptr = std::shared_ptr<IBarge>;
	using ConstPtr = std::shared_ptr<const IBarge>;

	virtual float velocity() const { return cargo ? cargoVelocity : ballastVelocity; }
	float draft() const override { return ballastDraft + draftBonus; }

	Timestamp dockingTime{ 0 };
	Timestamp riefPreparingTime{ 0 };

protected:
	float ballastDraft{ 0 };
	float ballastVelocity{ 0 };
	float cargoVelocity{ 0 };
	float draftBonus{ 0 };
};


struct DraftingOne
{
	void setDraftTable(ShipDraftTable::ConstPtr table) { draftTable = table; }

protected:
	float draftByTable(float cargo) const;

	ShipDraftTable::ConstPtr draftTable{ nullptr };
};

struct Barge : IBarge, DraftingOne
{
	using Ptr = std::shared_ptr<IBarge>;
	using ConstPtr = std::shared_ptr<const IBarge>;

	Barge()
	{
		ballastDraft = 3.f;
		draftBonus = 0.6f;
		ballastVelocity = 6.5f / 60;
		cargoVelocity = 5.5f / 60;
		capacity = 7200.f;
		dockingTime = 45;
		riefPreparingTime = 0;
	}
	virtual ~Barge() = default;

	float draft() const override { return (cargo ? draftByTable(cargo) : ballastDraft) + draftBonus; };

};

struct BTC : IBarge
{
	BTC()
	{
		ballastDraft = 0.9f;
		cargoDraft = 3.f;
		draftBonus = 0.3f;
		ballastVelocity = 5.5f / 60;
		cargoVelocity = 4.5f / 60;
		capacity = 12000.f;
		dockingTime = 45;
		riefPreparingTime = 45;
	}
	virtual ~BTC() = default;
	float draft() const override { return cargo ? cargoDraft : ballastDraft; }

private:
	float cargoDraft;
};

struct Washtub : IBarge
{
	Washtub()
	{
		ballastDraft = 0.96f;
		cargoDraft = 2.86f;
		draftBonus = 0.5f;
		capacity = 6000.f;
		dockingTime = 45;
		riefPreparingTime = 60;
	}
	virtual ~Washtub() = default;
	float draft() const override { return cargo ? cargoDraft : ballastDraft; }

private:
	float cargoDraft;
};

struct OceanVeichle : Ship
{
	using Ptr = std::shared_ptr<OceanVeichle>;
	using ConstPtr = std::shared_ptr<const OceanVeichle>;
	OceanVeichle()
	{
		capacity = 63e3;
	}
};

struct Tow : Ship
{
	using Ptr = std::shared_ptr<Tow>;
	using ConstPtr = std::shared_ptr<const Tow>;

	float towingVelocity = 4.f / 60;
	float movingVelocity = 5.f / 60;

	void load(float cargoToLoad) override;
    float unload() override;
	float draft() const override;

	int bargesTowed() const { return static_cast<int>(barges.size()); }
	std::vector<IBarge::Ptr> dropBarges();
	void towBarge(IBarge::Ptr barge);

	float velocity() const override { return bargesTowed() ? towingVelocity : movingVelocity; }
private:
	int maxBarges = 1;
	std::vector<IBarge::Ptr> barges;
	float ballastDraft = 1.26f;
};
