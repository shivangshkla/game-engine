
struct playing_sound
{
    v2 CurrentVolume;
    v2 dCurrentVolume;
    v2 TargetVolume;

    sound_id ID;
    f32 SamplesPlayed;
    f32 dSample;
    playing_sound *Next;
};

struct game_audio_state
{
    memory_arena *PermanentArena;
    playing_sound *FirstPlayingSound;
    playing_sound *FirstFreePlayingSound;
    v2 MasterVolume;
};
