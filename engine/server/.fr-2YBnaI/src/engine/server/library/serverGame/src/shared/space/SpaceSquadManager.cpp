// ======================================================================
//
// SpaceSquadManager.cpp
// Copyright Sony Online Entertainment, Inc.
//
// ======================================================================

#include "serverGame/FirstServerGame.h"
#include "serverGame/SpaceSquadManager.h"

#include "serverGame/AiShipController.h"
#include "serverGame/ConfigServerGame.h"
#include "serverGame/SpaceSquad.h"
#include "sharedFoundation/ExitChain.h"
#include "sharedFoundation/NetworkId.h"
#include "sharedLog/Log.h"

#include <map>

// ======================================================================
//
// SpaceSquadManagerNamespace
//
// ======================================================================

namespace SpaceSquadManagerNamespace
{
	typedef std::map<int, SpaceSquad *> SquadList;

	SquadList s_squadList;
	int s_nextSquadId = 1;

#ifdef _DEBUG
	void verifySquads();
	typedef std::vector<NetworkId> NetworkIdList;
	NetworkIdList s_debugUnitList;
#endif // _DEBUG
}

using namespace SpaceSquadManagerNamespace;

#ifdef _DEBUG
// ----------------------------------------------------------------------
void SpaceSquadManagerNamespace::verifySquads()
{
    {
        // Build a list of all the units

        s_debugUnitList.clear();

        // Using std::unordered_set for fast lookups
        std::unordered_set<NetworkId> uniqueUnits;
        
        for (SquadList::const_iterator iterSquadList = s_squadList.begin(); iterSquadList != s_squadList.end(); ++iterSquadList)
        {
            SpaceSquad const* squadPtr = iterSquadList->second;

            // Check if squadPtr is not null
            if (squadPtr == nullptr) 
            {
                DEBUG_WARNING(true, ("SpaceSquadManagerNamespace::verifySquads() ERROR: Found a null squad pointer for squad ID(%d).", iterSquadList->first));
                continue;
            }

            SpaceSquad const& squad = *squadPtr;
            SpaceSquad::UnitMap const& unitMap = squad.getUnitMap();
            
            for (SpaceSquad::UnitMap::const_iterator iterUnitMap = unitMap.begin(); iterUnitMap != unitMap.end(); ++iterUnitMap)
            {
                CachedNetworkId const& unit = iterUnitMap->first;

                // Insert unit into the set to keep track of all units
                uniqueUnits.insert(unit);
                s_debugUnitList.push_back(unit);
            }
        }

        // Ensure no duplicate units were collected
        if (uniqueUnits.size() != s_debugUnitList.size()) 
        {
            DEBUG_WARNING(true, ("SpaceSquadManagerNamespace::verifySquads() ERROR: Duplicate units detected."));
        }
    }

    {
        // Make sure each unit is in exactly 1 squad

        for (NetworkIdList::const_iterator iterDebugUnitList = s_debugUnitList.begin(); iterDebugUnitList != s_debugUnitList.end(); ++iterDebugUnitList)
        {
            NetworkId const& unit = (*iterDebugUnitList);
            int squadCount = 0;

            for (SquadList::const_iterator iterSquadList = s_squadList.begin(); iterSquadList != s_squadList.end(); ++iterSquadList)
            {
                SpaceSquad const* squadPtr = iterSquadList->second;

                // Check if squadPtr is not null
                if (squadPtr == nullptr)
                {
                    continue; // Skip if null
                }

                SpaceSquad const& squad = *squadPtr;

                if (squad.contains(unit))
                {
                    ++squadCount;
                }
            }

            // Warnings based on squad membership
            if (squadCount > 1)
            {
                DEBUG_WARNING(true, ("SpaceSquadManagerNamespace::verifySquads() unit(%s) ERROR: Unit is in more than one squad! attackSquadCount(%d)", unit.getValueString().c_str(), squadCount));
            }
            else if (squadCount != 1)
            {
                DEBUG_WARNING(true, ("SpaceSquadManagerNamespace::verifySquads() unit(%s) ERROR: Unit is not in a squad!", unit.getValueString().c_str()));
            }
        }
    }

    // Clear the debug list after verification to release memory
    s_debugUnitList.clear();
}

#endif // _DEBUG

// ======================================================================
//
// SpaceSquadManager
//
// ======================================================================

// ----------------------------------------------------------------------
void SpaceSquadManager::install()
{
	ExitChain::add(&remove, "SpaceSquadManager::remove");
}

// ----------------------------------------------------------------------
void SpaceSquadManager::remove()
{
    // Iterate through all the squads to check if they are empty and clean up
    for (SquadList::iterator iterSquadList = s_squadList.begin(); iterSquadList != s_squadList.end(); /* no increment here */)
    {
        // Check if the squad pointer is valid
        if (iterSquadList->second)
        {
            // Check if the squad is not empty
            if (!iterSquadList->second->isEmpty())
            {
                WARNING(false, ("SpaceSquadManager::remove() The squad(%d) is not empty(%u), this is a sign of a reference counting problem.", iterSquadList->first, iterSquadList->second->getUnitCount()));
            }
            else
            {
                // Clean up if the squad is empty
                delete iterSquadList->second;  // Free the memory of the squad
                iterSquadList = s_squadList.erase(iterSquadList);  // Erase and advance iterator
                continue;  // Skip increment as erase moves to the next element
            }
        }
        else
        {
            // Warning for invalid squad pointer
            WARNING(false, ("SpaceSquadManager::remove() Found a null squad pointer for squad ID(%d).", iterSquadList->first));
            // Erase the invalid pointer
            iterSquadList = s_squadList.erase(iterSquadList);
            continue;
        }

        ++iterSquadList; // Only increment if no erase
    }

    // Final check to see if the list is empty
    if (!s_squadList.empty())
    {
        WARNING(false, ("SpaceSquadManager::remove() The squad list is not empty(%u), this is a sign of a reference counting problem.", s_squadList.size()));
    }
}


// ----------------------------------------------------------------------
void SpaceSquadManager::alter(float const deltaTime)
{
	SquadList::iterator iterSquadList = s_squadList.begin();

	for (; iterSquadList != s_squadList.end();)
	{
		// Purge squads with no units

		if (iterSquadList->second->isEmpty())
		{
			LOGC(ConfigServerGame::isSpaceAiLoggingEnabled(), "space_debug_ai", ("SpaceSquadManager::alter() Purging empty squad(%d) totalSquadCount(%u)", iterSquadList->first, s_squadList.size() - 1));

			delete iterSquadList->second;

			s_squadList.erase(iterSquadList++);
		}
		else
		{

			iterSquadList->second->alter(deltaTime);
			++iterSquadList;

		}
	}

#ifdef _DEBUG
	verifySquads();
#endif // _DEBUG
}

// ----------------------------------------------------------------------
int SpaceSquadManager::createSquadId()
{
	LOGC(ConfigServerGame::isSpaceAiLoggingEnabled(), "space_debug_ai", ("SpaceSquadManager::createSquadId() newSquadId(%d)", s_nextSquadId));

	int const newId = s_nextSquadId;

	if (++s_nextSquadId == 0)
	{
		s_nextSquadId = 1;
	}

	return newId;
}

// ----------------------------------------------------------------------
SpaceSquad * SpaceSquadManager::createSquad()
{
	int const squadId = createSquadId();

	return createSquad(squadId);
}

// ----------------------------------------------------------------------
SpaceSquad * SpaceSquadManager::createSquad(int const squadId)
{
	FATAL((s_squadList.find(squadId) != s_squadList.end()), ("Trying to create a squad(%d) that already exists.", squadId));

	SpaceSquad * const squad = new SpaceSquad;

	squad->setId(squadId);

	IGNORE_RETURN(s_squadList.insert(std::make_pair(squadId, squad)));

	return squad;
}

// ----------------------------------------------------------------------
bool SpaceSquadManager::setSquadId(NetworkId const & unit, int const newSquadId)
{
	LOGC(ConfigServerGame::isSpaceAiLoggingEnabled(), "space_debug_ai", ("SpaceSquadManager::setSquadId() unit(%s) newSquadId(%d)", unit.getValueString().c_str(), newSquadId));

	bool result = false;
	AiShipController * const aiShipController = AiShipController::getAiShipController(unit);

	if (aiShipController != nullptr)
	{
		result = true;

		//-- See if the unit is already in the requested squad

		SpaceSquad const & oldSquad = aiShipController->getSquad();

		if (oldSquad.getId() != newSquadId)
		{
			SquadList::iterator iterSquadList = s_squadList.find(newSquadId);

			if (iterSquadList != s_squadList.end())
			{
				//-- The squad already existed, add the unit

				SpaceSquad * const newSquad = iterSquadList->second;

				newSquad->addUnit(unit);
			}
			else
			{
				//-- The squad did not exist, create it and add the unit

				SpaceSquad * const newSquad = SpaceSquadManager::createSquad(newSquadId);

				newSquad->addUnit(unit);
			}
		}
		else
		{
			LOGC(ConfigServerGame::isSpaceAiLoggingEnabled(), "space_debug_ai", ("SpaceSquadManager::setSquadId() unit(%s) newSquadId(%d) ERROR: The unit is already in the requested squad.", unit.getValueString().c_str(), newSquadId));
		}
	}
	else
	{
		LOGC(ConfigServerGame::isSpaceAiLoggingEnabled(), "space_debug_ai", ("SpaceSquadManager::setSquadId() unit(%s) newSquadId(%d) ERROR: Unable to resolve unit to an AiShipController.", unit.getValueString().c_str(), newSquadId));
	}

	return result;
}

// ----------------------------------------------------------------------
SpaceSquad * SpaceSquadManager::getSquad(int const squadId)
{
	SpaceSquad * result = nullptr;
	SquadList::const_iterator iterSpaceSquadList = s_squadList.find(squadId);

	if (iterSpaceSquadList != s_squadList.end())
	{
		result = iterSpaceSquadList->second;
	}

	return result;
}

// ======================================================================
