function playing_sound *
PlaySound(game_audio_state *AudioState, sound_id ID)
{
    if(!AudioState->FirstFreePlayingSound)
    {
        AudioState->FirstFreePlayingSound = PushStruct(AudioState->PermanentArena, playing_sound);
        AudioState->FirstFreePlayingSound->Next = 0;
    }

    playing_sound *PlayingSound = AudioState->FirstFreePlayingSound;
    AudioState->FirstFreePlayingSound = AudioState->FirstFreePlayingSound->Next;

    PlayingSound->CurrentVolume = PlayingSound->TargetVolume = V2(0.1f, 0.1f);
    PlayingSound->dCurrentVolume = V2(0, 0);
    PlayingSound->ID = ID;
    PlayingSound->SamplesPlayed = 0;
    PlayingSound->dSample = 1.0f;

    PlayingSound->Next = AudioState->FirstPlayingSound;
    AudioState->FirstPlayingSound = PlayingSound;

    return(PlayingSound);
}

function void
ChangeVolume(game_audio_state *AudioState, playing_sound *Sound, v2 Volume,
             f32 FadeDurationInSeconds)
{
    if(Sound)
    {
        if(FadeDurationInSeconds <= 0.0f)
        {
            Sound->CurrentVolume = Sound->TargetVolume = Volume;
        }
        else
        {
            Sound->TargetVolume = Volume;
            Sound->dCurrentVolume = (1.0f/FadeDurationInSeconds)*(Sound->TargetVolume - Sound->CurrentVolume);
        }
    }
}

function void
ChangePitch(game_audio_state *AudioState, playing_sound *Sound, f32 dSample)
{
    Sound->dSample = dSample;
}

function void
OutputPlayingSounds(game_audio *Audio, game_audio_state *AudioState, game_assets *Assets, memory_arena *TempArena)
{
    temporary_memory MixerMemory = BeginTemporaryMemory(TempArena);

    Assert((Audio->SampleCount & 3) == 0);
    s32 SampleCountAlign8 = Audio->SampleCount;
    s32 ChunkCount = SampleCountAlign8 / 4;

#define OutputChannelCount 2
    __m128 *RealChannel0 = PushArray(TempArena, ChunkCount, __m128, 16);
    __m128 *RealChannel1 = PushArray(TempArena, ChunkCount, __m128, 16);

    __m128 Zero = _mm_set1_ps(0.0f);
    __m128 One = _mm_set1_ps(1.0f);
    {
        __m128 *Dest0 = RealChannel0;
        __m128 *Dest1 = RealChannel1;

        for(s32 ChunkIndex = 0;
            ChunkIndex < ChunkCount;
            ++ChunkIndex)
        {
            _mm_store_ps((float *)Dest0++, Zero);
            _mm_store_ps((float *)Dest1++, Zero);
        }
    }

        for(playing_sound **PlayingSoundPtr = &AudioState->FirstPlayingSound;
        *PlayingSoundPtr;
        )
    {
        playing_sound *PlayingSound = *PlayingSoundPtr;

        b32 SoundFinished = false;

        __m128 *Dest0 = RealChannel0;
        __m128 *Dest1 = RealChannel1;

        s32 TotalChunksToMix = ChunkCount;
        while(TotalChunksToMix && !SoundFinished)
        {
            asset_slot *Slot = GetSoundSlot(Assets, PlayingSound->ID);
            if(Slot)
            {
                v2 Volume = PlayingSound->CurrentVolume;
                v2 dVolume = (1.0f / Audio->SampleRate)*PlayingSound->dCurrentVolume;
                v2 dVolumeChunk = 4.0f*dVolume;
                f32 dSample = PlayingSound->dSample*1.0f;
                f32 dSampleChunk = 4.0f*dSample;

                __m128 MasterVolume0 = _mm_set1_ps(AudioState->MasterVolume.X);
                __m128 Volume0 = _mm_setr_ps(Volume.E[0] + 0.0f*dVolume.E[0],
                                             Volume.E[0] + 1.0f*dVolume.E[0],
                                             Volume.E[0] + 2.0f*dVolume.E[0],
                                             Volume.E[0] + 3.0f*dVolume.E[0]);
                __m128 dVolume0 = _mm_set1_ps(dVolume.E[0]);
                __m128 dVolumeChunk0 = _mm_set1_ps(dVolumeChunk.E[0]);

                __m128 MasterVolume1 = _mm_set1_ps(AudioState->MasterVolume.Y);
                __m128 Volume1 = _mm_setr_ps(Volume.E[1] + 0.0f*dVolume.E[1],
                                             Volume.E[1] + 1.0f*dVolume.E[1],
                                             Volume.E[1] + 2.0f*dVolume.E[1],
                                             Volume.E[1] + 3.0f*dVolume.E[1]);
                __m128 dVolume1 = _mm_set1_ps(dVolume.E[1]);
                __m128 dVolumeChunk1 = _mm_set1_ps(dVolumeChunk.E[1]);

                Assert(PlayingSound->SamplesPlayed >= 0);

                s32 ChunksToMix = TotalChunksToMix;
                // NOTE(gaurang): Casey used FirstSampleIndex(truncated) here, was that a mistake?
                f32 FloatChunksRemainingInSound = (Slot->Sound.SampleCount - PlayingSound->SamplesPlayed) / dSampleChunk;
                s32 ChunksRemainingInSound = RoundReal32ToInt32(FloatChunksRemainingInSound);
                if(ChunksRemainingInSound < ChunksToMix)
                {
                    ChunksToMix = ChunksRemainingInSound;
                }

                s32 VolumeEndsAt[OutputChannelCount] = {};
                for(s32 ChannelIndex = 0;
                    ChannelIndex < OutputChannelCount;
                    ++ChannelIndex)
                {
                    if(dVolumeChunk.E[ChannelIndex] != 0.0f)
                    {
                        f32 DeltaVolume = PlayingSound->TargetVolume.E[ChannelIndex] - Volume.E[ChannelIndex];
                        s32 VolumeChunkCount = (s32)((DeltaVolume / dVolumeChunk.E[ChannelIndex]) + 0.5f);
                        if(ChunksToMix > VolumeChunkCount)
                        {
                            ChunksToMix = VolumeChunkCount;
                            VolumeEndsAt[ChannelIndex] = VolumeChunkCount;
                        }
                    }
                }

                f32 BeginSamplePosition = PlayingSound->SamplesPlayed;
                f32 EndSamplePosition = BeginSamplePosition + ChunksToMix*dSampleChunk;
                f32 LoopIndexC = (EndSamplePosition - BeginSamplePosition) / (f32)ChunksToMix;
                for(s32 LoopIndex = 0;
                    LoopIndex < ChunksToMix;
                    ++LoopIndex)
                {
                    f32 SamplePosition = BeginSamplePosition + LoopIndexC*(f32)LoopIndex;
#if 1
                    __m128 SamplePos = _mm_setr_ps(SamplePosition + 0.0f*dSample,
                                                   SamplePosition + 1.0f*dSample,
                                                   SamplePosition + 2.0f*dSample,
                                                   SamplePosition + 3.0f*dSample);
                    __m128i SampleIndex = _mm_cvttps_epi32(SamplePos);
                    __m128 Frac = _mm_sub_ps(SamplePos, _mm_cvtepi32_ps(SampleIndex));
                    __m128 OneMinusFrac = _mm_sub_ps(One, Frac);

                    __m128 SampleValueF = _mm_setr_ps(Slot->Sound.Samples[0][((int *)&SampleIndex)[0]],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[1]],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[2]],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[3]]);
                    __m128 SampleValueC = _mm_setr_ps(Slot->Sound.Samples[0][((int *)&SampleIndex)[0] + 1],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[1] + 1],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[2] + 1],
                                                      Slot->Sound.Samples[0][((int *)&SampleIndex)[3] + 1]);

                    __m128 SampleValue = _mm_add_ps(_mm_mul_ps(OneMinusFrac, SampleValueF),
                                                    _mm_mul_ps(Frac, SampleValueC));
#else
                    __m128 SampleValue = _mm_setr_ps(Slot->Sound.Samples[0][RoundReal32ToInt32(SamplePosition + 0.0f*dSample)],
                                                     Slot->Sound.Samples[0][RoundReal32ToInt32(SamplePosition + 1.0f*dSample)],
                                                     Slot->Sound.Samples[0][RoundReal32ToInt32(SamplePosition + 2.0f*dSample)],
                                                     Slot->Sound.Samples[0][RoundReal32ToInt32(SamplePosition + 3.0f*dSample)]);
#endif
                    __m128 D0 = _mm_load_ps((float *)&Dest0[0]);
                    __m128 D1 = _mm_load_ps((float *)&Dest1[0]);

                    D0 = _mm_add_ps(D0, _mm_mul_ps(_mm_mul_ps(MasterVolume0, Volume0), SampleValue));
                    D1 = _mm_add_ps(D1, _mm_mul_ps(_mm_mul_ps(MasterVolume1, Volume1), SampleValue));

                    _mm_store_ps((float *)&Dest0[0], D0);
                    _mm_store_ps((float *)&Dest1[0], D1);

                    Dest0 += 1;
                    Dest1 += 1;

                    Volume0 = _mm_add_ps(Volume0, dVolumeChunk0);
                    Volume1 = _mm_add_ps(Volume1, dVolumeChunk1);
                }

                PlayingSound->CurrentVolume.E[0] = ((f32 *)&Volume0)[0];
                PlayingSound->CurrentVolume.E[1] = ((f32 *)&Volume0)[1];

                for(s32 ChannelIndex = 0;
                    ChannelIndex < OutputChannelCount;
                    ++ChannelIndex)
                {
                    if(ChunksToMix == VolumeEndsAt[ChannelIndex])
                    {
                            PlayingSound->CurrentVolume.E[ChannelIndex] = PlayingSound->TargetVolume.E[ChannelIndex];
                            PlayingSound->dCurrentVolume.E[ChannelIndex] = 0.0f;
                    }
                }

                PlayingSound->SamplesPlayed = EndSamplePosition;
                Assert(TotalChunksToMix >= ChunksToMix);
                TotalChunksToMix -= ChunksToMix;

                s32 SamplesPlayed = FloorReal32ToInt32(PlayingSound->SamplesPlayed);
                if(ChunksToMix == ChunksRemainingInSound)
                {
                    SoundFinished = true;
                }
                else
                {
                    // Assert(TotalSamplesToMix == 0);
                }
            }
            else
            {
                LoadSound(Assets, PlayingSound->ID);
                break;
            }
        }

        if(SoundFinished)
        {
            *PlayingSoundPtr = PlayingSound->Next;
            PlayingSound->Next = AudioState->FirstFreePlayingSound;
            AudioState->FirstFreePlayingSound = PlayingSound;
        }
        else
        {
            PlayingSoundPtr = &PlayingSound->Next;
        }
    }

    {
        __m128 *Source0 = RealChannel0;
        __m128 *Source1 = RealChannel1;

        __m128i *SampleOut = (__m128i *)Audio->Samples;
        for(s32 SampleIndex = 0;
            SampleIndex < ChunkCount;
            ++SampleIndex)
        {
#if 1
            __m128 S0 = _mm_load_ps((float *)Source0++);
            __m128 S1 = _mm_load_ps((float *)Source1++);
#else
            __m128 S0 = *Source0++;
            __m128 S1 = *Source1++;
#endif
            __m128i L = _mm_cvtps_epi32(S0);
            __m128i R = _mm_cvtps_epi32(S1);

            __m128i LR0 = _mm_unpacklo_epi32(L, R);
            __m128i LR1 = _mm_unpackhi_epi32(L, R);

            __m128i S01 = _mm_packs_epi32(LR0, LR1);

            // *SampleOut++ = S01;
            _mm_store_si128(SampleOut++, S01);
        }
    }

    EndTemporaryMemory(MixerMemory);
}

function void
InitializeAudioState(game_audio_state *AudioState, memory_arena *PermanentArena)
{
    AudioState->PermanentArena = PermanentArena;
    AudioState->FirstPlayingSound = 0;
    AudioState->FirstFreePlayingSound = 0;
    AudioState->MasterVolume = V2(1.0f, 1.0f);
}
