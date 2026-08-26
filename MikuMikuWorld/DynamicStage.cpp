#include "DynamicStage.h"
#include "Constants.h"
#include <choc/memory/choc_xxHash.h>
#include <cstring>

namespace MikuMikuWorld
{
	id_t nextStageID = 1;
	id_t nextCameraChangeID = 1;
	id_t nextStageMaskChangeID = 1;
	id_t nextStagePivotChangeID = 1;
	id_t nextStageStyleChangeID = 1;
	id_t nextStageTransformChangeID = 1;

	id_t getNextStageID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextStageID, sizeof(id_t));
		nextStageID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 4);
		return nextStageID;
	}

	id_t getNextCameraChangeID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextCameraChangeID, sizeof(id_t));
		nextCameraChangeID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 5);
		return nextCameraChangeID;
	}

	id_t getNextStageMaskChangeID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextStageMaskChangeID, sizeof(id_t));
		nextStageMaskChangeID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 6);
		return nextStageMaskChangeID;
	}

	id_t getNextStagePivotChangeID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextStagePivotChangeID, sizeof(id_t));
		nextStagePivotChangeID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 7);
		return nextStagePivotChangeID;
	}

	id_t getNextStageStyleChangeID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextStageStyleChangeID, sizeof(id_t));
		nextStageStyleChangeID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 8);
		return nextStageStyleChangeID;
	}

	id_t getNextStageTransformChangeID()
	{
		uint8_t data[sizeof(id_t)];
		std::memcpy(data, &nextStageTransformChangeID, sizeof(id_t));
		nextStageTransformChangeID = choc::hash::xxHash64::hash(&data, sizeof(id_t), HASH_SEED + 9);
		return nextStageTransformChangeID;
	}
}