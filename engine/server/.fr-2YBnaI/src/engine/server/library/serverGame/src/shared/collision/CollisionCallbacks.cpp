//  ════════════════════════════════════════════════════════════════════════
//   File         : CollisionCallbacks.cpp
//   Author       : tford / patrick
//   Description  : Collision detection callbacks for SWG+ spaceflight.
//
//   © 2004-2025 Sony Online Entertainment + SWG+ Dev Team
//  ════════════════════════════════════════════════════════════════════════


#include "serverGame/FirstServerGame.h"
#include "serverGame/CollisionCallbacks.h"

#include "serverGame/ShipController.h"
#include "serverGame/ShipObject.h"
#include "sharedFoundation/CrcLowerString.h"
#include "sharedGame/CollisionCallbackManager.h"
#include "sharedGame/SharedObjectTemplate.h"
#include "sharedNetworkMessages/MessageQueueUpdateShipOnCollision.h"
#include "sharedMath/Transform.h"
#include "sharedMathArchive/TransformArchive.h"
#include "sharedNetworkMessages/ShipUpdateTransformCollisionMessage.h"
#include "sharedObject/CellProperty.h"

// ======================================================================

namespace CollisionCallbacksNamespace
{
	void remove();
	int convertObjectToIndex(Object * const object);
	bool onHitDoCollisionWith(Object * const object, Object * const wasHitByThisObject);
	bool onDoCollisionWithTerrain(Object * const object);
}

using namespace CollisionCallbacksNamespace;

// ======================================================================

void CollisionCallbacks::install()
{
	CollisionCallbackManager::install();

	CollisionCallbackManager::registerConvertObjectToIndexFunction(CollisionCallbacksNamespace::convertObjectToIndex);

	int const shipFighter = static_cast<int>(SharedObjectTemplate::GOT_ship_fighter);
	int const shipCapital = static_cast<int>(SharedObjectTemplate::GOT_ship_capital);
	int const shipStation = static_cast<int>(SharedObjectTemplate::GOT_ship_station);
	int const shipTransport = static_cast<int>(SharedObjectTemplate::GOT_ship_transport);
	int const asteroid = static_cast<int>(SharedObjectTemplate::GOT_misc_asteroid);
	int const miningAsteroidStatic = static_cast<int>(SharedObjectTemplate::GOT_ship_mining_asteroid_static);
	int const miningAsteroidDynamic = static_cast<int>(SharedObjectTemplate::GOT_ship_mining_asteroid_dynamic);

	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, shipCapital);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, shipStation);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, shipTransport);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, asteroid);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, miningAsteroidStatic);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipFighter, miningAsteroidDynamic);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipCapital, shipCapital);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipCapital, shipStation);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipCapital, shipTransport);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipCapital, asteroid);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, shipCapital);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, shipStation);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, shipTransport);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, asteroid);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, miningAsteroidStatic);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, shipTransport, miningAsteroidDynamic);

	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, miningAsteroidDynamic, miningAsteroidStatic);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, miningAsteroidDynamic, shipCapital);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, miningAsteroidDynamic, shipTransport);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, miningAsteroidDynamic, asteroid);
	CollisionCallbackManager::registerOnHitFunction(CollisionCallbacksNamespace::onHitDoCollisionWith, miningAsteroidDynamic, shipStation);

	//CollisionCallbackManager::registerDoCollisionWithTerrainFunction(CollisionCallbacksNamespace::onDoCollisionWithTerrain);

	ExitChain::add(CollisionCallbacksNamespace::remove, "CollisionCallbacks");
}

// ======================================================================

void CollisionCallbacksNamespace::remove()
{ }

// ----------------------------------------------------------------------

int CollisionCallbacksNamespace::convertObjectToIndex(Object * const object)
{
	FATAL(!object, ("CollisionCallbacksNamespace::convertObjectToIndex: object == nullptr."));

	ServerObject const * serverObject = object->asServerObject();
	if (serverObject)
		return serverObject->getGameObjectType();

	return SharedObjectTemplate::GOT_none;
}

// ----------------------------------------------------------------------

bool CollisionCallbacksNamespace::onHitDoCollisionWith(Object * const object, Object * const wasHitByThisObject)
{
	DEBUG_WARNING(!object, ("CollisionCallbacksNamespace::onHitDoCollisionWith: Object == nullptr"));
	DEBUG_WARNING(!wasHitByThisObject, ("CollisionCallbacksNamespace::onHitDoCollisionWith: wasHitByThisObject == nullptr"));

	ShipObject * shipObject = safe_cast<ShipObject *>(object);
	DEBUG_WARNING(!shipObject, ("CollisionCallbacksNamespace::onHitDoCollisionWith: shipObject == nullptr"));

	CollisionCallbackManager::Result result;
	if (CollisionCallbackManager::intersectAndReflect(object, wasHitByThisObject, result))
	{
		ShipController * const shipController = static_cast<ShipController *>(shipObject->getController());
		NOT_NULL(shipController);

		shipController->respondToCollision(result.m_deltaToMoveBack_p, result.m_newReflection_p, result.m_normalOfSurface_p);

		return true;
	}

	return false;
}

// ----------------------------------------------------------------------
bool CollisionCallbacksNamespace::onDoCollisionWithTerrain(Object * const object)
{
	DEBUG_FATAL(!object, ("CollisionCallbacksNamespace::onDoCollisionWithTerrain: Object == nullptr"));

	ShipObject * shipObject = safe_cast<ShipObject *>(object);
	DEBUG_FATAL(!shipObject, ("CollisionCallbacksNamespace::onDoCollisionWithTerrain: shipObject == nullptr"));

	CollisionCallbackManager::Result result;
	if (CollisionCallbackManager::intersectAndReflectWithTerrain(object, result))
	{
		ShipController * const shipController = safe_cast<ShipController *>(shipObject->getController());
		NOT_NULL(shipController);

		Vector const & correction = result.m_deltaToMoveBack_p;

		// Only apply correction if it's non-zero
		if (correction.magnitudeSquared() > 0.0001f)
		{
			Vector const currentPos = object->getPosition_p();
			Vector const correctedPos = currentPos + correction;

			object->setPosition_p(correctedPos);  // forcibly move out of terrain
		}

		// Handle reflection / response
		shipController->respondToCollision(result.m_deltaToMoveBack_p, result.m_newReflection_p, result.m_normalOfSurface_p);

		DEBUG_REPORT_LOG(true, ("[Terrain Collision] Ship %s corrected to avoid terrain overlap.\n", object->getNetworkId().getValueString().c_str()));

		return true;
	}

	return false;
}

// ======================================================================

