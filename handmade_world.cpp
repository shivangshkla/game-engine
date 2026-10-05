//
// TODO(gaurang): Do these really belong in more of a "positioning" or
// "geometry" file?
//
function world_position
RecanonicalizePosition(world *World, world_position P)
{
    world_position Result = P;

    s32 dChunkX = FloorReal32ToInt32(Result.Offset_.X / World->ChunkSideInMeters.X);
    s32 dChunkY = FloorReal32ToInt32(Result.Offset_.Y / World->ChunkSideInMeters.Y);
    s32 dChunkZ = FloorReal32ToInt32(Result.Offset_.Z / World->ChunkSideInMeters.Z);

    Result.ChunkX += dChunkX;
    Result.ChunkY += dChunkY;
    Result.ChunkZ += dChunkZ;

    Result.Offset_.X -= dChunkX*World->ChunkSideInMeters.X;
    Result.Offset_.Y -= dChunkY*World->ChunkSideInMeters.Y;
    Result.Offset_.Z -= dChunkZ*World->ChunkSideInMeters.Z;

    Assert(Result.Offset_.X >= 0.0f);
    Assert(Result.Offset_.Y >= 0.0f);
    Assert(Result.Offset_.Z >= 0.0f);
    Assert(Result.Offset_.X < World->ChunkSideInMeters.X);
    Assert(Result.Offset_.Y < World->ChunkSideInMeters.Y);
    Assert(Result.Offset_.Z < World->ChunkSideInMeters.Z);

    return(Result);
}

function world_position
Offset(world *World, world_position P, v3 Offset)
{
    world_position Result = P;
    Result.Offset_ += Offset;

    Result = RecanonicalizePosition(World, Result);

    return(Result);
}

function v3
Subtract(world *World , world_position A, world_position B)
{
    v3 Result;

    Result.X = (A.ChunkX - B.ChunkX)*World->ChunkSideInMeters.X + (A.Offset_.X - B.Offset_.X);
    Result.Y = (A.ChunkY - B.ChunkY)*World->ChunkSideInMeters.Y + (A.Offset_.Y - B.Offset_.Y);
    Result.Z = (A.ChunkZ - B.ChunkZ)*World->ChunkSideInMeters.Z + (A.Offset_.Z - B.Offset_.Z);

    return(Result);
}

function b32
AreOnSameTile(world *World, world_position A, world_position B)
{
    b32 Result = false;

    if((A.ChunkX == B.ChunkX) && (A.ChunkY == B.ChunkY) && (A.ChunkZ == B.ChunkZ))
    {
        s32 ATileX = FloorReal32ToInt32(A.Offset_.X / World->TileSideInMeters);
        s32 ATileY = FloorReal32ToInt32(A.Offset_.Y / World->TileSideInMeters);
        s32 ATileZ = FloorReal32ToInt32(A.Offset_.Z / World->TileSideInMeters);

        s32 BTileX = FloorReal32ToInt32(B.Offset_.X / World->TileSideInMeters);
        s32 BTileY = FloorReal32ToInt32(B.Offset_.Y / World->TileSideInMeters);
        s32 BTileZ = FloorReal32ToInt32(B.Offset_.Z / World->TileSideInMeters);

        if((ATileX == BTileX) && (ATileY == BTileY) && (ATileZ == BTileZ))
        {
            Result = true;
        }
    }

    return(Result);
}

function world_position
WorldPositionFromTilePosition(world *World, s32 TileX, s32 TileY, s32 TileZ)
{
    world_position Result = { 0 };
    Result.Offset_.X = TileX * World->TileSideInMeters + 0.5f*World->TileSideInMeters;
    Result.Offset_.Y = TileY * World->TileSideInMeters + 0.5f*World->TileSideInMeters;
    Result.Offset_.Z = TileZ * World->ChunkSideInMeters.Z;
    Result = RecanonicalizePosition(World, Result);
    return(Result);
}

//

function chunk *
GetChunk(world *World, s32 ChunkX, s32 ChunkY, s32 ChunkZ, memory_arena *Arena = 0)
{
    // TODO(gaurang): BETTER HASH FUNCTION!!!
    s32 HashValue = 19*ChunkX + 7*ChunkY + 3*ChunkZ;
    s32 HashSlot = HashValue & (ArrayCount(World->ChunkHash) - 1);
    Assert(HashSlot < ArrayCount(World->ChunkHash));

    chunk **Chunk = World->ChunkHash + HashSlot;
    for(;
        *Chunk;
        Chunk = &(*Chunk)->NextInHash)
    {
        if((ChunkX == (*Chunk)->X) &&
           (ChunkY == (*Chunk)->Y) &&
           (ChunkZ == (*Chunk)->Z))
        {
            break;
        }
    }

    if(Arena && !(*Chunk))
    {
        *Chunk = PushStruct(Arena, chunk);
        (*Chunk)->X = ChunkX;
        (*Chunk)->Y = ChunkY;
        (*Chunk)->Z = ChunkZ;
    }

    return(*Chunk);
}

function void
ChangeEntityLocation(memory_arena *Arena, world *World, s32 StoredEntityIndex, world_position NewP)
{
    Assert(StoredEntityIndex);
	// TODO(gaurang): Should we avoid doing this if the entity already
	// exists and is in the same chunk? We need to find it either way
	// to confirm its existence, so the work is not wasted, if we
	// allow others to track them, they may make a mistake? The
	// current system is very robust.
	stored_entity *Entity = World->StoredEntities + StoredEntityIndex;
	chunk *Chunk = GetChunk(World, Entity->P.ChunkX, Entity->P.ChunkY, Entity->P.ChunkZ);
	if(Chunk)
	{
		chunk_stored_entity_index_block *FirstBlock = Chunk->FirstBlock;
		for(chunk_stored_entity_index_block *Block = FirstBlock;
			Block;
			Block = Block->Next)
		{
			for(s32 Index = 0;
				Index < Block->StoredEntityIndexCount;
				++Index)
			{
				if(StoredEntityIndex == Block->StoredEntityIndex[Index])
				{
					--FirstBlock->StoredEntityIndexCount;
					Block->StoredEntityIndex[Index] = FirstBlock->StoredEntityIndex[FirstBlock->StoredEntityIndexCount];

					if(FirstBlock->StoredEntityIndexCount == 0)
					{
						Chunk->FirstBlock = FirstBlock->Next;
						FirstBlock->Next = World->FirstFree;
						World->FirstFree = FirstBlock;
					}

					goto ExitLoop;
				}
			}
		}
	ExitLoop:;
	}

	Chunk = GetChunk(World, NewP.ChunkX, NewP.ChunkY, NewP.ChunkZ, Arena);
	Assert(Chunk);
	chunk_stored_entity_index_block *FirstBlock = Chunk->FirstBlock;
	Assert((!FirstBlock) || (FirstBlock->StoredEntityIndexCount <= ArrayCount(FirstBlock->StoredEntityIndex)));
	if((!FirstBlock) || (FirstBlock->StoredEntityIndexCount == ArrayCount(FirstBlock->StoredEntityIndex)))
	{
		chunk_stored_entity_index_block *Block = World->FirstFree;
		if(Block)
		{
			World->FirstFree = World->FirstFree->Next;
		}
		else
		{
			Block = PushStruct(Arena, chunk_stored_entity_index_block);
		}
		Block->Next = FirstBlock;
		FirstBlock = Block;
		Chunk->FirstBlock = FirstBlock;
	}
	FirstBlock->StoredEntityIndex[FirstBlock->StoredEntityIndexCount++] = StoredEntityIndex;
	Entity->P = NewP;
}

function s32
AddStoredEntity(memory_arena *Arena, world *World, world_position P)
{
	Assert(World->StoredEntityCount < ArrayCount(World->StoredEntities));
	s32 Index = World->StoredEntityCount++;

	ChangeEntityLocation(Arena, World, Index, P);
	World->StoredEntities[Index].P = P;

	return(Index);
}

function s32
AddHero(memory_arena *Arena, world *World, s32 TileX, s32 TileY, s32 TileZ)
{
	world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->HeroCollision;
	Entity->Entity.HitPointMax = 3;
	Entity->Entity.HitPointCount = 2;
	Entity->Entity.Type = EntityType_Hero;
	Entity->Entity.StoredIndex = EntityIndex;
	AddFlags(&Entity->Entity, EntityFlag_Movable);
	AddFlags(&Entity->Entity, EntityFlag_Collides);

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
	return(EntityIndex);
}

function s32
AddFamiliar(memory_arena *Arena, world *World, s32 TileX, s32 TileY, s32 TileZ)
{
	world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->FamiliarCollision;
	Entity->Entity.Type = EntityType_Familiar;
	Entity->Entity.StoredIndex = EntityIndex;

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
	return(EntityIndex);
}

function s32
AddMonster(memory_arena *Arena, world *World,
		   s32 TileX, s32 TileY, s32 TileZ)
{

    world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->MonsterCollision;
	Entity->Entity.HitPointMax = 6;
	Entity->Entity.HitPointCount = 4;
	Entity->Entity.Type = EntityType_Monster;
	Entity->Entity.StoredIndex = EntityIndex;
	AddFlags(&Entity->Entity, EntityFlag_Collides);

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
	return(EntityIndex);
}


function void
AddWall(memory_arena *Arena, world *World, s32 TileX, s32 TileY, s32 TileZ)
{
    world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->WallCollision;
	Entity->Entity.Type = EntityType_Wall;
	Entity->Entity.StoredIndex = EntityIndex;
	AddFlags(&Entity->Entity, EntityFlag_Collides);

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
}

function void
AddStair(memory_arena *Arena, world *World, s32 TileX, s32 TileY, s32 TileZ)
{
	world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->StairCollision;
	Entity->Entity.Type = EntityType_Stair;
	Entity->Entity.StoredIndex = EntityIndex;
	Entity->Entity.WalkableDimension = Entity->Entity.CollisionGroup->TotalVolume.Dimension.XY;
	Entity->Entity.WalkableHeight = World->ChunkSideInMeters.Z;
	AddFlags(&Entity->Entity, EntityFlag_Collides);

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
}

function void
AddStandardRoom(memory_arena *Arena, world *World,
				s32 MinTileX, s32 MinTileY, s32 MinTileZ,
				s32 MaxTileX, s32 MaxTileY, s32 MaxTileZ)
{
	s32 TileX = MinTileX + TruncateReal32ToInt32((MaxTileX - MinTileX)/2.0f);
	s32 TileY = MinTileY + TruncateReal32ToInt32((MaxTileY - MinTileY)/2.0f);
	s32 TileZ = MinTileZ + TruncateReal32ToInt32((MaxTileZ - MinTileZ)/2.0f);

	world_position P = WorldPositionFromTilePosition(World, TileX, TileY, TileZ);
	s32 EntityIndex = AddStoredEntity(Arena, World, P);

	stored_entity *Entity = World->StoredEntities + EntityIndex;
	Entity->Entity.CollisionGroup = World->StandardRoomCollision;
	Entity->Entity.Type = EntityType_Space;
	Entity->Entity.StoredIndex = EntityIndex;
	AddFlags(&Entity->Entity, EntityFlag_Traversable);

	ChangeEntityLocation(Arena, World, EntityIndex, Entity->P);
}

function entity_collision_volume_group *
MakeSimpleGroundedCollision(game_state *GameState, v3 Dimension)
{
	// TODO(gaurang): NOT WORLD ARENA! Change to using fundamental
	// types arena, etc.
	entity_collision_volume_group *Group = PushStruct(&GameState->WorldArena, entity_collision_volume_group);
	Group->VolumeCount = 1;
	Group->Volumes = PushArray(&GameState->WorldArena, Group->VolumeCount, entity_collision_volume);
	Group->TotalVolume.OffsetP = V3(0, 0, 0.5f*Dimension.Z);
	Group->TotalVolume.Dimension = Dimension;
	Group->Volumes[0] = Group->TotalVolume;
	return(Group);
}
