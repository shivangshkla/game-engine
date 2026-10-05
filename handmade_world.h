enum entity_type
{
	EntityType_Null,
	EntityType_Wall,
	EntityType_Hero,
	EntityType_Stair,
	EntityType_Familiar,
	EntityType_Monster,
	EntityType_Space,
	EntityType_Sword,
};

enum entity_flags
{
	EntityFlag_Collides = (1 << 0),
	EntityFlag_NonSpatial = (1 << 1),
	EntityFlag_Movable = (1 << 2),
	EntityFlag_ZSupported = (1 << 3),
	EntityFlag_Traversable = (1 << 4),
};

struct entity_collision_volume
{
	v3 OffsetP;
	v3 Dimension;
};

struct entity_collision_volume_group
{
	entity_collision_volume TotalVolume;
	s32 VolumeCount;
	entity_collision_volume *Volumes;
};

struct pairwise_collision_rule
{
	b32 CanCollide;
	s32 StoredIndexA;
	s32 StoredIndexB;
	pairwise_collision_rule *NextInHash;
};

struct entity
{
	v3 P;
	v3 dP;
	entity_collision_volume_group *CollisionGroup;
	r32 tBob;
	s32 HitPointMax;
	s32 HitPointCount;
	r32 DistanceLimit;
	f32 FacingDirection;
	s32 ControllerIndex;

	entity *Sword;

	entity_type Type;
	u32 Flags;

	v2 WalkableDimension;
	r32 WalkableHeight;

	// NOTE(gaurang): These are for sim region.
	b32 Updatable;
	s32 StoredIndex;
};

function b32
IsSet(entity *Entity, u32 Flag)
{
	b32 Result = (Entity->Flags & Flag);
	return(Result);
}

function void
AddFlags(entity *Entity, u32 Flag)
{
	Entity->Flags |= Flag;
}

function void
ClearFlags(entity *Entity, u32 Flag)
{
	Entity->Flags &= ~Flag;
}

struct world_position
{
    s32 ChunkX;
    s32 ChunkY;
    s32 ChunkZ;

    v3 Offset_;
};

struct stored_entity
{
	world_position P;
	s32 SwordIndex;
	entity Entity;
};

struct chunk_stored_entity_index_block
{
    s32 StoredEntityIndexCount;
    s32 StoredEntityIndex[16];
    chunk_stored_entity_index_block *Next;
};

struct chunk
{
    s32 X;
    s32 Y;
    s32 Z;

    chunk_stored_entity_index_block *FirstBlock;

    chunk *NextInHash;
};

struct world
{
    r32 TileSideInMeters;
    v3 ChunkSideInMeters;

    // NOTE(gaurang): At the moment, this must be a power of 2!
    chunk *ChunkHash[4096];
    chunk_stored_entity_index_block *FirstFree;

    s32 StoredEntityCount;
    stored_entity StoredEntities[4096];

    pairwise_collision_rule *CollisionRuleHash[256];
	pairwise_collision_rule *FirstFreeCollisionRule;

    entity_collision_volume_group *SwordCollision;
	entity_collision_volume_group *StairCollision;
	entity_collision_volume_group *HeroCollision;
	entity_collision_volume_group *MonsterCollision;
	entity_collision_volume_group *WallCollision;
	entity_collision_volume_group *FamiliarCollision;
	entity_collision_volume_group *StandardRoomCollision;
};
