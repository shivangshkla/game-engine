struct entity_hash
{
	s32 StoredEntityIndex;
	entity *Entity;
};

struct sim_region
{
	r32 MaxEntityRadius;
	r32 MaxEntityVelocity;

	world_position Origin;
	rectangle3 Bounds;
	rectangle3 UpdatableBounds;

	s32 EntityCount;
	entity Entities[1024];
	entity_hash StoredIndexToSimPointerHash[1024];
};
