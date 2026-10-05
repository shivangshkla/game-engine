//
// TODO(gaurang): Convert all of these to platform-efficient versions
// and remove math.h
//

#include <math.h>
#if COMPILER_MSVC
#include <intrin.h>
#define CompletePastWritesBeforeFutureWrites _WriteBarrier()
#define CompletePastReadsBeforeFutureReads _ReadBarrier()
function s32 AtomicCompareExchangeInt32(s32 volatile *Value, s32 New, s32 Expected)
{
    s32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);
    return(Result);
}
function s32 AtomicAdd(s32 volatile *Addend, s32 Value)
{
    s32 Result = _InterlockedExchangeAdd((long volatile *)Addend, Value);
    return(Result);
}

function u32 AtomicCompareExchangeUInt32(u32 volatile *Value, u32 New, u32 Expected)
{
    u32 Result = _InterlockedCompareExchange((long volatile *)Value, New, Expected);
    return(Result);
}
function u32 AtomicAdd(u32 volatile *Addend, u32 Value)
{
    u32 Result = _InterlockedExchangeAdd((long volatile *)Addend, Value);
    return(Result);
}
#endif

function f32
SignOf(f32 Value)
{
    f32 Result = (Value >= 0) ? 1.0f : -1.0f;
    return(Result);
}

function s32
SignOf(s32 Value)
{
    s32 Result = (Value >= 0) ? 1 : -1;
    return(Result);
}

function r32
Square(r32 A)
{
    r32 Result = A*A;
    return(Result);
}

function r32
SquareRoot(r32 A)
{
    r32 Result = sqrtf(A);
    return(Result);
}

function r32
AbsoluteValue(r32 A)
{
    r32 Result = fabsf(A);
    return(Result);
}

function s32
TruncateReal32ToInt32(r32 Value)
{
    s32 Result = (s32)Value;
    return(Result);
}

function u32
TruncateReal32ToUInt32(r32 Value)
{
    Assert(Value >= 0.0f);
    u32 Result = (u32)Value;
    return(Result);
}

function s32
FloorReal32ToInt32(r32 Value)
{
    s32 Result = (s32)floorf(Value);
    return(Result);
}

function s32
CeilReal32ToInt32(r32 Value)
{
    s32 Result = (s32)ceilf(Value);
    return(Result);
}

function s32
RoundReal32ToInt32(r32 Value)
{
    s32 Result = (s32)roundf(Value);
    return(Result);
}

function r32
Sin(r32 Angle)
{
    r32 Result = sinf(Angle);
    return(Result);
}

function r32
Cos(r32 Angle)
{
    r32 Result = cosf(Angle);
    return(Result);
}

function r32
ATan2(r32 Y, r32 X)
{
    r32 Result = atan2f(Y, X);
    return(Result);
}

struct bit_scan_result
{
    b32 Found;
    s32 Index;
};

function bit_scan_result
FindLeastSignificantSetBit(u32 Value)
{
    bit_scan_result Result = {};
#if COMPILER_MSVC
    Result.Found = _BitScanForward((unsigned long *)&Result.Index, Value);
#else
    for(s32 Test = 0;
        Test < 32;
        ++Test)
    {
        if(Value & (1 << Test))
        {
            Result.Index = Test;
            Result.Found = true;
            break;
        }
    }
#endif

    return(Result);
}

function u32
RotateLeft(u32 Value, s32 Amount)
{
    Amount &= 31;
    u32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
    return(Result);
}

function u32
RotateRight(u32 Value, s32 Amount)
{
    Amount &= 31;
    u32 Result = ((Value >> Amount) | (Value << (32 - Amount)));
    return(Result);
}

