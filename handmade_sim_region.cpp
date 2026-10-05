function entity *
GetEntity(sim_region *Region, s32 Index)
{
	Assert(Index < Region->EntityCount);
	entity *Entity = Region->Entities + Index;
	return(Entity);
}

function void
ClearCollisionRule(world *World, s32 StoredIndex)
{
	for(s32 HashBucket = 0;
		HashBucket < ArrayCount(World->CollisionRuleHash);
		++HashBucket)
	{
		for(pairwise_collision_rule **Rule = &World->CollisionRuleHash[HashBucket];
			*Rule;
			)
		{
			if(((*Rule)->StoredIndexA == StoredIndex) ||
			   ((*Rule)->StoredIndexB == StoredIndex))
			{
				pairwise_collision_rule *RemovedRule = (*Rule);
				*Rule = (*Rule)->NextInHash;

				RemovedRule->NextInHash = World->FirstFreeCollisionRule;
				World->FirstFreeCollisionRule = RemovedRule;
			}
			else
			{
				Rule = &(*Rule)->NextInHash;
			}
		}
	}
}

function void
AddCollisionRule(memory_arena *WorldArena, world *World, s32 StoredIndexA, s32 StoredIndexB, b32 CanCollide)
{
	if(StoredIndexA > StoredIndexB)
	{
		s32 Temp = StoredIndexA;
		StoredIndexA = StoredIndexB;
		StoredIndexB = Temp;
	}

	u32 HashBucket = (StoredIndexA & StoredIndexB) & (ArrayCount(World->CollisionRuleHash)-1);
	pairwise_collision_rule *Found = 0;
	for(pairwise_collision_rule *Rule = World->CollisionRuleHash[HashBucket];
		Rule;
		Rule = Rule->NextInHash)
	{
		if((Rule->StoredIndexA == StoredIndexA) &&
		   (Rule->StoredIndexB == StoredIndexB))
		{
			Found = Rule;
			break;
		}
	}

	if(!Found)
	{
		Found = World->FirstFreeCollisionRule;
		if(Found)
		{
			World->FirstFreeCollisionRule = World->FirstFreeCollisionRule->NextInHash;
			ZeroStruct(*Found);
		}
		else
		{
			Found = PushStruct(WorldArena, pairwise_collision_rule);
		}
		Found->NextInHash = World->CollisionRuleHash[HashBucket];
		World->CollisionRuleHash[HashBucket] = Found;
	}

	Assert(Found);

	Found->StoredIndexA = StoredIndexA;
	Found->StoredIndexB = StoredIndexB;
	Found->CanCollide = CanCollide;
}

function b32
CanCollide(world *World, entity *A, entity *B)
{
	b32 Result = false;

	if(A != B)
	{
		if(A->StoredIndex > B->StoredIndex)
		{
			entity *Temp = A;
			A = B;
			B = Temp;
		}

		if(IsSet(A, EntityFlag_Collides) &&
		   IsSet(B, EntityFlag_Collides))
		{
			if(!IsSet(A, EntityFlag_NonSpatial) &&
			   !IsSet(B, EntityFlag_NonSpatial))
			{
				Result = true;
			}

			u32 HashBucket = (A->StoredIndex & B->StoredIndex) & (ArrayCount(World->CollisionRuleHash)-1);
			for(pairwise_collision_rule *Rule = World->CollisionRuleHash[HashBucket];
				Rule;
				Rule = Rule->NextInHash)
			{
				if((Rule->StoredIndexA == A->StoredIndex) &&
				   (Rule->StoredIndexB == B->StoredIndex))
				{
					Result = Rule->CanCollide;
					break;
				}
			}
		}
	}

	return(Result);
}

function b32
HandleCollisions(memory_arena *WorldArena, world *World, entity *A, entity *B)
{
	if(A->Type > B->Type)
	{
		entity *Temp = A;
		A = B;
		B = Temp;
	}

	b32 StopOnCollision = true;
	if(B->Type == EntityType_Sword)
	{
		StopOnCollision = false;
		AddCollisionRule(WorldArena, World, A->StoredIndex, B->StoredIndex, false);
	}

	if((A->Type == EntityType_Monster) &&
	   (B->Type == EntityType_Sword))
	{
		if(A->HitPointCount > 0)
		{
			A->HitPointCount -= 1;
		}
	}

	return(StopOnCollision);
}

function b32
CanOverlap(entity *Mover, entity *Region)
{
	b32 Result = false;

	if(Mover != Region)
	{
		if(Region->Type == EntityType_Stair)
		{
			Result = true;
		}
	}

	return(Result);
}

function v3
GetEntityGroundPoint(entity *Entity)
{
	v3 Result = Entity->P;
	return(Result);
}

function r32
GetStairGround(entity *Region, v3 AtGroundPoint)
{
	Assert(Region->Type == EntityType_Stair);
	rectangle2 RegionRect = RectCenterDim(Region->P.XY, Region->WalkableDimension);
	v2 Bary = Clamp01(GetBarycentric(RegionRect, AtGroundPoint.XY));
	r32 Ground = Region->P.Z + Bary.Y*Region->WalkableHeight;
	return(Ground);
}

function void
HandleOverlap(entity *Mover, entity *Region, r32 dtForFrame, r32 *Ground)
{
	if(Region->Type == EntityType_Stair)
	{
		*Ground = GetStairGround(Region, GetEntityGroundPoint(Mover));
	}
}

function b32
SpeculativeCollide(entity *Mover, entity *Region)
{
	b32 Result = true;

	if(Region->Type == EntityType_Stair)
	{
		r32 Ground = GetStairGround(Region, GetEntityGroundPoint(Mover));
		r32 StepHeight = 0.1f;
#if 0
		Result = ((AbsoluteValue(GetEntityGroundPoint(Mover).Z - Ground) > StepHeight) ||
				  ((Bary.Y > 0.01f) && (Bary.Y < 0.99f)));
#else
		Result = ((AbsoluteValue(GetEntityGroundPoint(Mover).Z - Ground) > StepHeight));
#endif
	}

	return(Result);
}

function b32
EntitiesOverlap(entity *Entity, entity *TargetEntity, v3 Epsilon = V3(0, 0, 0))
{
	b32 Overlapped = false;
	for(s32 EntityVolumeIndex = 0;
		!Overlapped && EntityVolumeIndex < Entity->CollisionGroup->VolumeCount;
		++EntityVolumeIndex)
	{
		entity_collision_volume *EntityVolume = Entity->CollisionGroup->Volumes + EntityVolumeIndex;
		rectangle3 EntityRectangle = RectCenterDim(Entity->P + EntityVolume->OffsetP, EntityVolume->Dimension + Epsilon);
		for(s32 TargetEntityVolumeIndex = 0;
			!Overlapped && TargetEntityVolumeIndex < TargetEntity->CollisionGroup->VolumeCount;
			++TargetEntityVolumeIndex)
		{
			entity_collision_volume *TargetVolume = TargetEntity->CollisionGroup->Volumes + TargetEntityVolumeIndex;
			rectangle3 TargetRectangle = RectCenterDim(TargetEntity->P + TargetVolume->OffsetP, TargetVolume->Dimension);
			if(RectanglesIntersect(EntityRectangle, TargetRectangle))
			{
				Overlapped = true;
			}
		}
	}
	return(Overlapped);
}

struct test_wall
{
	r32 X;
	r32 DeltaX;
	r32 DeltaY;
	r32 MinY;
	r32 MaxY;
	v3 Normal;
};

function b32
TestWall(r32 *tMin, r32 X, r32 DeltaX, r32 DeltaY, r32 MinY, r32 MaxY)
{
	b32 Collides = false;

	r32 Epsilon = 0.001f;
	if(DeltaX != 0.0f)
	{
		r32 tResult = X / DeltaX;
		r32 Y = tResult*DeltaY;
		if((tResult > 0.0f) && (tResult < *tMin))
		{
			if((Y >= MinY) && (Y < MaxY))
			{
				*tMin = Maximum(0.0f, tResult - Epsilon);
				Collides = true;
			}
		}
	}

	return(Collides);
}

struct move_spec
{
	b32 UnitMaxAccelVector;
	r32 Acceleration;
	r32 Drag;
};

function void
MoveEntity(game_state *GameState, sim_region *SimRegion, s32 EntityIndex, r32 dtForFrame, move_spec *MoveSpec, v3 ddP)
{
	entity *Entity = GetEntity(SimRegion, EntityIndex);

	Assert(MoveSpec);
	if(MoveSpec->UnitMaxAccelVector)
	{
		r32 ddPLength = SquareRoot(LengthSq(ddP));
		if(ddPLength > 1.0f)
		{
			ddP *= 1.0f/ddPLength;
		}
	}
	ddP *= MoveSpec->Acceleration;

	// STUDY(gaurang): TODO(gaurang): Should we use the new velocity
	// or the old velocity for friction? ODE here!
	ddP.XY += -MoveSpec->Drag*Entity->dP.XY;
	if(!IsSet(Entity, EntityFlag_ZSupported))
	{
		ddP += V3(0, 0, -9.8f);
	}

	v3 DeltaP = 0.5f*ddP*Square(dtForFrame) + Entity->dP*dtForFrame;
	Entity->dP = ddP*dtForFrame + Entity->dP;

	// TODO(gaurang): Upgrade physical motion routines to handle
	// capping the maximum velocity?
	Assert(LengthSq(Entity->dP) <= Square(SimRegion->MaxEntityVelocity));

	r32 DistanceRemaining = Entity->DistanceLimit;
	if(DistanceRemaining == 0.0f)
	{
		DistanceRemaining = 10000.0f;
	}

	for(s32 Iteration = 0;
		Iteration < 4;
		++Iteration)
	{
		r32 tMin = 1.0f;
		r32 tMax = 0.0f;
		v3 WallNormalMin = { 0 };
		v3 WallNormalMax = { 0 };
		entity *HitEntityMin = 0;
		entity *HitEntityMax = 0;

		r32 DeltaPLength = Length(DeltaP);
		if(DeltaPLength > 0.0f)
		{
			if(DeltaPLength > DistanceRemaining)
			{
				tMin *= (DistanceRemaining / DeltaPLength);
			}

			// NOTE(gaurang): Just an early out, optimization.
			if(!IsSet(Entity, EntityFlag_NonSpatial))
			{
				for(s32 Index = 1;
					Index < SimRegion->EntityCount;
					++Index)
				{
					entity *TargetEntity = SimRegion->Entities + Index;

					v3 OverlapEpsilon = V3(0.001f, 0.001f, 0.001f);
					if((IsSet(TargetEntity, EntityFlag_Traversable) &&
						EntitiesOverlap(Entity, TargetEntity, OverlapEpsilon)) ||
					   CanCollide(GameState->World, Entity, TargetEntity))
					{
						for(s32 EntityVolumeIndex = 0;
							EntityVolumeIndex < Entity->CollisionGroup->VolumeCount;
							++EntityVolumeIndex)
						{
							entity_collision_volume *EntityVolume = Entity->CollisionGroup->Volumes + EntityVolumeIndex;
							for(s32 TargetEntityVolumeIndex = 0;
								TargetEntityVolumeIndex < TargetEntity->CollisionGroup->VolumeCount;
								++TargetEntityVolumeIndex)
							{
								entity_collision_volume *TargetVolume = TargetEntity->CollisionGroup->Volumes + TargetEntityVolumeIndex;
								v3 Radius = 0.5f*(TargetVolume->Dimension + EntityVolume->Dimension);
								v3 TargetP = (TargetEntity->P + TargetVolume->OffsetP) - (Entity->P + EntityVolume->OffsetP);
								v3 MinCorner = TargetP - Radius;
								v3 MaxCorner = TargetP + Radius;

								if((MinCorner.Z <= 0.0f) && (MaxCorner.Z > 0.0f))
								{
									test_wall Walls[] = {
										{ MinCorner.X, DeltaP.X, DeltaP.Y, MinCorner.Y, MaxCorner.Y, V3(-1, 0, 0) },
										{ MaxCorner.X, DeltaP.X, DeltaP.Y, MinCorner.Y, MaxCorner.Y, V3(1, 0, 0) },
										{ MinCorner.Y, DeltaP.Y, DeltaP.X, MinCorner.X, MaxCorner.X, V3(0, -1, 0) },
										{ MaxCorner.Y, DeltaP.Y, DeltaP.X, MinCorner.X, MaxCorner.X, V3(0, 1, 0) },
									};

									if(IsSet(TargetEntity, EntityFlag_Traversable))
									{
										entity *TestHitEntity = 0;
										r32 tMaxTest = tMax;
										v3 TestWallNormal = { 0 };

										for(s32 WallIndex = 0;
											WallIndex < ArrayCount(Walls);
											++WallIndex)
										{
											test_wall *Wall = Walls + WallIndex;

											r32 Epsilon = 0.001f;
											if(Wall->DeltaX != 0.0f)
											{
												r32 tResult = Wall->X / Wall->DeltaX;
												r32 Y = tResult*Wall->DeltaY;
												if((tResult > 0.0f) && (tResult > tMaxTest))
												{
													if((Y >= Wall->MinY) && (Y < Wall->MaxY))
													{
														tMaxTest = Maximum(0.0f, tResult - Epsilon);
														TestWallNormal = Wall->Normal;
														TestHitEntity = TargetEntity;
													}
												}
											}
										}

										if(TestHitEntity)
										{
											tMax = tMaxTest;
											WallNormalMax = TestWallNormal;
											HitEntityMax = TestHitEntity;
										}
									}
									else
									{
										entity *TestHitEntity = 0;
										r32 tMinTest = tMin;
										v3 TestWallNormal = { 0 };

										for(s32 WallIndex = 0;
											WallIndex < ArrayCount(Walls);
											++WallIndex)
										{
											test_wall *Wall = Walls + WallIndex;

											r32 Epsilon = 0.001f;
											if(Wall->DeltaX != 0.0f)
											{
												r32 tResult = Wall->X / Wall->DeltaX;
												r32 Y = tResult*Wall->DeltaY;
												if((tResult > 0.0f) && (tResult < tMinTest))
												{
													if((Y >= Wall->MinY) && (Y < Wall->MaxY))
													{
														tMinTest = Maximum(0.0f, tResult - Epsilon);
														TestWallNormal = Wall->Normal;
														TestHitEntity = TargetEntity;
													}
												}
											}
										}

										if(TestHitEntity)
										{
											if(SpeculativeCollide(Entity, TestHitEntity))
											{
												tMin = tMinTest;
												WallNormalMin = TestWallNormal;
												HitEntityMin = TestHitEntity;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			r32 tStop = 0.0f;
			entity *HitEntity = 0;
			v3 WallNormal = { 0 };
			if(tMin < tMax)
			{
				tStop = tMin;
				HitEntity = HitEntityMin;
				WallNormal = WallNormalMin;
			}
			else
			{
				tStop = tMax;
				HitEntity = HitEntityMax;
				WallNormal = WallNormalMax;
			}

			Entity->P += tStop*DeltaP;
			DeltaP = DeltaP - tStop*DeltaP;
			DistanceRemaining += -tStop*DeltaPLength;

			if(HitEntity)
			{
				b32 StopsOnCollision = HandleCollisions(&GameState->WorldArena, GameState->World, Entity, HitEntity);
				if(StopsOnCollision)
				{
					DeltaP = DeltaP - Inner(DeltaP, WallNormal)*WallNormal;
					Entity->dP = Entity->dP - Inner(Entity->dP, WallNormal)*WallNormal;
				}
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	r32 Ground = 0.0f;
	// NOTE(gaurang): Handle events based on area overlapping.
	{
		for(s32 Index = 1;
			Index < SimRegion->EntityCount;
			++Index)
		{
			entity *TargetEntity = SimRegion->Entities + Index;

			if(CanOverlap(Entity, TargetEntity) &&
			   EntitiesOverlap(Entity, TargetEntity))
			{
				HandleOverlap(Entity, TargetEntity, dtForFrame, &Ground);
			}
		}
	}

	Ground += Entity->P.Z - GetEntityGroundPoint(Entity).Z;
	if(Entity->P.Z <= Ground ||
	   (IsSet(Entity, EntityFlag_ZSupported) &&
		Entity->dP.Z == 0.0f))
	{
		Entity->P.Z = Ground;
		Entity->dP.Z = 0;
		AddFlags(Entity, EntityFlag_ZSupported);
	}
	else
	{
		ClearFlags(Entity, EntityFlag_ZSupported);
	}

	if(Entity->DistanceLimit != 0.0f)
	{
		Entity->DistanceLimit = DistanceRemaining;
	}
}

//

function b32
EntityOverlapsRectangle(v3 P, entity_collision_volume Volume, rectangle3 Rectangle)
{
	rectangle3 Grown = AddRadiusTo(Rectangle, 0.5f*Volume.Dimension);
	b32 Result = IsInRectangle(Grown, P + Volume.OffsetP);
	return(Result);
}

function entity_hash *
GetEntityHashFromStoredIndex(sim_region *Region, s32 StoredEntityIndex)
{
	entity_hash *Result = 0;

	u32 HashValue = StoredEntityIndex;
	for(s32 Offset = 0;
		Offset < ArrayCount(Region->StoredIndexToSimPointerHash);
		++Offset)
	{
		entity_hash *Entry = Region->StoredIndexToSimPointerHash +
			((HashValue + Offset) & (ArrayCount(Region->StoredIndexToSimPointerHash) - 1)) ;
		if((Entry->StoredEntityIndex == 0) || (Entry->StoredEntityIndex == StoredEntityIndex))
		{
			Result = Entry;
			break;
		}
	}

	return(Result);
}

function void
MapStoredIndexToEntity(sim_region *Region, s32 StoredEntityIndex, entity *Entity)
{
	entity_hash *Entry = GetEntityHashFromStoredIndex(Region, StoredEntityIndex);
	Assert((Entry->StoredEntityIndex == 0) || (Entry->StoredEntityIndex == StoredEntityIndex));
	Entry->StoredEntityIndex = StoredEntityIndex;
	Entry->Entity = Entity;
}

function entity *
StoredToEntity(sim_region *Region, world *World, s32 StoredEntityIndex);
function entity *
ResolveEntityReference(sim_region *Region, world *World, s32 StoredEntityIndex)
{
	entity *Result = 0;
	if(StoredEntityIndex)
	{
		entity_hash *Entry = GetEntityHashFromStoredIndex(Region, StoredEntityIndex);
		if(Entry->StoredEntityIndex == 0)
		{
			Result = StoredToEntity(Region, World, StoredEntityIndex);
		}
		else
		{
			Assert(Entry->StoredEntityIndex == StoredEntityIndex);
			Result = Entry->Entity;
		}
	}
	return(Result);
}

function entity *
StoredToEntity(sim_region *Region, world *World, s32 StoredEntityIndex)
{
	entity_hash *Entry = GetEntityHashFromStoredIndex(Region, StoredEntityIndex);
	entity *Entity = Entry->Entity;
	if(Entry->StoredEntityIndex == 0)
	{
		stored_entity *StoredEntity = World->StoredEntities + StoredEntityIndex;
		s32 SimIndex = Region->EntityCount++;
		Entity = Region->Entities + SimIndex;
		*Entity = StoredEntity->Entity;
		v3 EntityP = Subtract(World, StoredEntity->P, Region->Origin);

		if(IsSet(&StoredEntity->Entity, EntityFlag_NonSpatial))
		{
			EntityP = V3(100000.0f, 100000.0f, 100000.0f);
		}
		else
		{
			// TODO(gaurang): Should we allow entities to hold
			// references Spatial entities outside the sim region?
			// Currently, if our sword went out of the updatable
			// bound, it will stop updating, and will never become
			// nonspatial, if it goes out of SimRegion bounds this
			// assert will get triggered, because player's sword
			// reference is no longer present.
			Assert(EntityOverlapsRectangle(EntityP, Entity->CollisionGroup->TotalVolume, Region->Bounds));
		}

		if(EntityOverlapsRectangle(EntityP, Entity->CollisionGroup->TotalVolume, Region->UpdatableBounds))
		{
			Entity->Updatable = true;
		}

		Entity->P = EntityP;
		Entity->Sword = ResolveEntityReference(Region, World, StoredEntity->SwordIndex);
		Entity->StoredIndex = StoredEntityIndex;

		MapStoredIndexToEntity(Region, StoredEntityIndex, Entity);
	}
	else
	{
		Assert(Entry->StoredEntityIndex == StoredEntityIndex);
	}

	return(Entity);
}

function entity *
NewEntity(memory_arena *Arena, world *World, sim_region *Region, v3 P, entity_collision_volume_group *CollisionGroup)
{
	s32 StoredIndex = AddStoredEntity(Arena, World, Offset(World, Region->Origin, P));
	World->StoredEntities[StoredIndex].Entity.CollisionGroup = CollisionGroup;
	entity *Entity = StoredToEntity(Region, World, StoredIndex);

	return(Entity);
}

function sim_region *
BeginSim(memory_arena *SimArena, world *World, world_position Origin, rectangle3 Bounds, r32 dtForFrame)
{
	sim_region *Region = PushStruct(SimArena, sim_region);
	ZeroStruct(*Region);
	Region->MaxEntityRadius = 5.0f;
	Region->MaxEntityVelocity = 30.0f;
	Region->Origin = Origin;
	Region->UpdatableBounds = Bounds;
    Region->Bounds = AddRadiusTo(Region->UpdatableBounds, (Region->MaxEntityRadius + dtForFrame*Region->MaxEntityVelocity) * V3(1, 1, 1));

	// NOTE(gaurang): Null entity.
	Region->EntityCount = 1;

	world_position Min = Offset(World, Origin, Region->Bounds.Min);
	world_position Max = Offset(World, Origin, Region->Bounds.Max);

	for(s32 ChunkZ = Min.ChunkZ;
		ChunkZ <= Max.ChunkZ;
		++ChunkZ)
	{
		for(s32 ChunkY = Min.ChunkY;
			ChunkY <= Max.ChunkY;
			++ChunkY)
		{
			for(s32 ChunkX = Min.ChunkX;
				ChunkX <= Max.ChunkX;
				++ChunkX)
			{
				chunk *Chunk = GetChunk(World, ChunkX, ChunkY, ChunkZ);
				if(Chunk)
				{
					for(chunk_stored_entity_index_block *Block = Chunk->FirstBlock;
						Block;
						Block = Block->Next)
					{
						for(s32 StoredEntityIndexIndex = 0;
							StoredEntityIndexIndex < Block->StoredEntityIndexCount;
							++StoredEntityIndexIndex)
						{
							s32 StoredEntityIndex = Block->StoredEntityIndex[StoredEntityIndexIndex];
							stored_entity *StoredEntity = World->StoredEntities + StoredEntityIndex;

							v3 EntityP = Subtract(World, StoredEntity->P, Region->Origin);

							if(EntityOverlapsRectangle(EntityP, StoredEntity->Entity.CollisionGroup->TotalVolume, Region->Bounds))
							{
								StoredToEntity(Region, World, StoredEntityIndex);
							}
						}
					}
				}
			}
		}
	}

	return(Region);
}

function void
EndSim(memory_arena *WorldArena, world *World, sim_region *Region)
{
	for(s32 Index = 1;
		Index < Region->EntityCount;
		++Index)
	{
		entity *Entity = Region->Entities + Index;
		s32 StoredEntityIndex = Entity->StoredIndex;
		stored_entity *StoredEntity = World->StoredEntities + StoredEntityIndex;

		StoredEntity->Entity = *Entity;
		StoredEntity->SwordIndex = 0;
		StoredEntity->Entity.Updatable = false;
		if(Entity->Sword)
		{
			StoredEntity->SwordIndex = Entity->Sword->StoredIndex;
		}

		world_position NewP = Offset(World, Region->Origin, Entity->P);
		Assert(StoredEntityIndex);
		ChangeEntityLocation(WorldArena, World, StoredEntityIndex, NewP);
	}
}
