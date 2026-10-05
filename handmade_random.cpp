function random_series
Seed(s32x Index)
{
    random_series Result;

    Result.RandomNumberIndex = Index % ArrayCount(RandomNumberTable_);

    return(Result);
}

function s32x
NextRandomNumber(random_series *Series)
{
    s32x Result = RandomNumberTable_[Series->RandomNumberIndex++] - MinRandomNumber;

    Series->RandomNumberIndex %= ArrayCount(RandomNumberTable_);

    return(Result);
}

function s32x
RandomChoice(random_series *Series, s32x ChoiceCount)
{
    s32x Result = NextRandomNumber(Series) % ChoiceCount;
    return(Result);
}

function f32
RandomUnilateral(random_series *Series)
{
    f32 Divisor = 1.0f / (MaxRandomNumber - MinRandomNumber);
    f32 Result = Divisor*NextRandomNumber(Series);
    return(Result);
}

function f32
RandomBilateral(random_series *Series)
{
    f32 Result = 2.0f*RandomUnilateral(Series) - 1.0f;
    return(Result);
}

function f32
RandomBetween(random_series *Series, f32 Min, f32 Max)
{
    f32 Result = Lerp(Min, RandomUnilateral(Series), Max);
    return(Result);
}

function s32x
RandomBetween(random_series *Series, s32x Min, s32x Max)
{
    s32x Result = Min + NextRandomNumber(Series) % ((Max + 1) - Min);
    return(Result);
}
