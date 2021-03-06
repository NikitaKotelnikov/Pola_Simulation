#pragma once

#include "core.h"

struct Ship
{
	enum class State { LOADING, UNLOADING, GOING_LOAD, GOING_UNLOAD, WAITING, RIEF_PASSED, DOCKING, MOVING, ERROR };
	using Ptr = std::shared_ptr<Ship>;
	using ConstPtr = std::shared_ptr<const Ship>;

	void rememberState(State askedState, std::string comment = "", bool rewrite = false);

	template <typename T>
	void printHistory(T& stream) const
	{
		stream << "\nLog of \"" << id << "\":\n";
		for (auto &shipEvent : history)
			stream << id << "\t" <<printTime(shipEvent.first) << "\t" << shipEvent.second << "\n";
    stream << "Mileage: " << mileage << "\n";
	  if (loadsCounter) stream <<"Loads counter: " << loadsCounter << "\n";
	}

	Timestamp grepHistory(const std::string &str);

	virtual void load(float cargoToLoad);
	virtual float unload();
	Timestamp move(float dist);
	
  virtual float draft() const { return ballastDraft + draftBonus; }
  virtual float velocity() const { throw std::logic_error("Not implemented, stupido!"); }

	State getState() const { return state; }

	void clear();

  bool towable = false;
  bool anchored = true;
  
  static std::string printState(State st);

protected:
	inline void synchroTime(Ship::Ptr ship)
	{
		/*if (ship->localTimer > localTimer) // it's okay - we're waiting
			rememberState(Ship::State::ERROR, "Timestamp above " + ship->id);*/

		auto synchroTime = std::max(ship->localTimer, localTimer);
		localTimer = synchroTime;
		ship->localTimer = synchroTime;
	}

protected:
	std::multimap < Timestamp, std::string > history;
	State state{ State::LOADING };

public:
	std::string id = "UNNAMED";
	Timestamp localTimer = 0;

	float capacity{ 0.f };
	float cargo{ 0.f };
  
  float ballastDraft{ 0 };
  float draftBonus{ 0 };

  float ballastVelocity{ 0 };
  float mileage{ 0 };

	int loadsCounter = 0;
};



struct IBarge : Ship
{
	using Ptr = std::shared_ptr<IBarge>;
	using ConstPtr = std::shared_ptr<const IBarge>;

	float velocity() const override { return cargo ? cargoVelocity : ballastVelocity; }

	Timestamp dockingTime{ 45 };
	Timestamp undockingTime{ 30 };
	Timestamp anchoringTime{ 30 };
	Timestamp unanchoringTime{ 45 };

protected:

	float cargoVelocity{ 0 };
};


struct DraftingOne
{
	void setDraftTable(ShipDraftTable::ConstPtr table) { draftTable = table; }

protected:
	float draftByTable(float cargo) const;

	ShipDraftTable::ConstPtr draftTable{ nullptr };
};

struct RSD : IBarge, DraftingOne
{
	using Ptr = std::shared_ptr<IBarge>;
	using ConstPtr = std::shared_ptr<const IBarge>;

	RSD()
	{
		ballastDraft = 3.f;
		draftBonus = 0.6f;
		ballastVelocity = 6.5f / 60;
		cargoVelocity = 5.5f / 60;
		capacity = 7200.f;
    towable = false;
	}
	virtual ~RSD() = default;

	float draft() const override { return (cargo ? draftByTable(cargo) : ballastDraft) + draftBonus; };

};

//struct BTC : IBarge
//{
//	BTC()
//	{
//		ballastDraft = 0.9f;
//		cargoDraft = 3.f;
//		draftBonus = 0.3f;
//		ballastVelocity = 5.5f / 60;
//		cargoVelocity = 4.5f / 60;
//		capacity = 12000.f;
//
//    dockingTime *= 2;
//    undockingTime *= 2;
//	}
//	virtual ~BTC() = default;
//	float draft() const override { return cargo ? cargoDraft : ballastDraft; }
//
//private:
//	float cargoDraft;
//};

struct Washtub : IBarge
{
	Washtub(int multipler = 1)
	{
		ballastDraft = 0.9f;
		cargoDraft = 2.86f;
		draftBonus = 0.5f;
		capacity = multipler * 6000.f;
		dockingTime = multipler * 45;
     undockingTime = multipler * 30 ;

     anchoringTime = 30 ;
     unanchoringTime = 45;

    if (multipler > 1)
    {
      dockingTime += multipler * anchoringTime;
      undockingTime += multipler * unanchoringTime;
    }
     towable = true;

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
  virtual ~OceanVeichle() = default;

};

struct Tow : Ship
{
	using Ptr = std::shared_ptr<Tow>;
	using ConstPtr = std::shared_ptr<const Tow>;

  virtual ~Tow() = default;
  Tow()
  {
    ballastDraft = 3.75;
    draftBonus = 0.3;
  }

	float towingVelocity = 4.5f / 60;
	float movingVelocity = 5.5f / 60;

	void load(float cargoToLoad) override;
  float unload() override;
	float draft() const override;

	int bargesTowed() const { return static_cast<int>(barges.size()); }
	std::vector<IBarge::Ptr> dropBarges(Timestamp operationTime);
	void towBarge(IBarge::Ptr barge, Timestamp operationTime);
	void synchronizeTimestamps() { for (auto barge : barges) synchroTime(barge); }

	float velocity() const override { return bargesTowed() ? towingVelocity : movingVelocity; }

private:
	int maxBarges = 1;
	std::vector<IBarge::Ptr> barges;
};

struct LoadingOrder
{
	LoadingOrder(float cargo = 5000, float intensity = 2000 / 60) : loadCargo(cargo), loadIntensity(intensity) {}
	float loadCargo;
	float loadIntensity;
};

struct ShipContext
{
	IBarge::Ptr ship = nullptr;
	std::queue<LoadingOrder> orders;
};
