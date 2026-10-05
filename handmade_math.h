function r32
Lerp(r32 A, r32 t, r32 B)
{
    r32 Result = (1.0f - t)*A + t*B;
    return(Result);
}

function r32
Clamp(r32 Min, r32 Value, r32 Max)
{
    r32 Result = Value;

    if(Result < Min)
    {
        Result = Min;
    }
    else if(Result > Max)
    {
        Result = Max;
    }

    return(Result);
}

function r32
Clamp01(r32 Value)
{
    r32 Result = Clamp(0.0f, Value, 1.0f);
    return(Result);
}

function r32
Clamp01MapToRange(r32 Min, r32 t, r32 Max)
{
    r32 Result = 0.0f;

    r32 Range = Max - Min;
    if(Range != 0.0f)
    {
        Result = Clamp01((t - Min) / Range);
    }

    return(Result);
}

function r32
SafeRatioN(r32 Numerator, r32 Denominator, r32 N)
{
    r32 Result = N;

    if(Denominator != 0.0f)
    {
        Result = Numerator / Denominator;
    }

    return(Result);
}

function r32
SafeRatio0(r32 Numerator, r32 Denominator)
{
    r32 Result = SafeRatioN(Numerator, Denominator, 0.0f);

    return(Result);
}

function r32
SafeRatio1(r32 Numerator, r32 Denominator)
{
    r32 Result = SafeRatioN(Numerator, Denominator, 1.0f);

    return(Result);
}

union v2
{
    struct
    {
        r32 X;
        r32 Y;
    };
    struct
    {
        r32 U;
        r32 V;
    };
    r32 E[2];
};

function v2
V2i(s32 X, s32 Y)
{
    v2 Result;
    Result.X = (r32)X;
    Result.Y = (r32)Y;
    return(Result);
}

function v2
V2i(u32 X, u32 Y)
{
    v2 Result;
    Result.X = (r32)X;
    Result.Y = (r32)Y;
    return(Result);
}

function v2
V2(r32 X, r32 Y)
{
    v2 Result;

    Result.X = X;
    Result.Y = Y;

    return(Result);
}

function v2
operator-(v2 A)
{
    v2 Result;

    Result.X = -A.X;
    Result.Y = -A.Y;

    return(Result);
}

function v2
operator+(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;

    return(Result);
}

function v2
operator-(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;

    return(Result);
}

function v2
operator*(v2 A, r32 B)
{
    v2 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;

    return(Result);
}

function v2
operator*(r32 B, v2 A)
{
    v2 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;

    return(Result);
}

function v2
operator+=(v2 &A, v2 B)
{
    A.X += B.X;
    A.Y += B.Y;

    return(A);
}

function v2
operator-=(v2 &A, v2 B)
{
    A.X -= B.X;
    A.Y -= B.Y;

    return(A);
}

function v2
operator*=(v2 &A, r32 B)
{
    A.X *= B;
    A.Y *= B;

    return(A);
}

function v2
Hadamard(v2 A, v2 B)
{
    v2 Result;

    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;

    return(Result);
}

function r32
Inner(v2 A, v2 B)
{
    r32 Result = A.X*B.X + A.Y*B.Y;
    return(Result);
}

function r32
LengthSq(v2 A)
{
    r32 Result = Inner(A, A);
    return(Result);
}

function r32
Length(v2 A)
{
    r32 Result = SquareRoot(Inner(A, A));
    return(Result);
}

function v2
Perp(v2 A)
{
    v2 Result;

    Result.X = -A.Y;
    Result.Y = A.X;

    return(Result);
}

function v2
Clamp01(v2 A)
{
    v2 Result;

    Result.X = Clamp01(A.X);
    Result.Y = Clamp01(A.Y);

    return(Result);
}

union v3
{
    struct
    {
        r32 X;
        r32 Y;
        r32 Z;
    };
    struct
    {
        r32 R;
        r32 G;
        r32 B;
    };
    struct
    {
        r32 U;
        r32 V;
        r32 W;
    };
    struct
    {
        v2 XY;
        r32 Ignored0_;
    };
    struct
    {
        r32 Ignored1_;
        v2 YZ;
    };
    struct
    {
        v2 UV;
        r32 Ignored2_;
    };
    r32 E[3];
};

function v3
V3(r32 X, r32 Y, r32 Z)
{
    v3 Result;

    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;

    return(Result);
}

function v3
V3(v2 XY, r32 Z)
{
    v3 Result;

    Result.X = XY.X;
    Result.Y = XY.Y;
    Result.Z = Z;

    return(Result);
}

function v3
ToV3(v2 XY, r32 Z)
{
    v3 Result;

    Result.X = XY.X;
    Result.Y = XY.Y;
    Result.Z = Z;

    return(Result);
}

function v3
operator-(v3 A)
{
    v3 Result;

    Result.X = -A.X;
    Result.Y = -A.Y;
    Result.Z = -A.Z;

    return(Result);
}

function v3
operator+(v3 A, v3 B)
{
    v3 Result;

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;

    return(Result);
}

function v3
operator-(v3 A, v3 B)
{
    v3 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;

    return(Result);
}

function v3
operator*(v3 A, r32 B)
{
    v3 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;
    Result.Z = A.Z * B;

    return(Result);
}

function v3
operator*(r32 B, v3 A)
{
    v3 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;
    Result.Z = A.Z * B;

    return(Result);
}

function v3
operator+=(v3 &A, v3 B)
{
    A.X += B.X;
    A.Y += B.Y;
    A.Z += B.Z;

    return(A);
}

function v3
operator-=(v3 &A, v3 B)
{
    A.X -= B.X;
    A.Y -= B.Y;
    A.Z -= B.Z;

    return(A);
}

function v3
operator*=(v3 &A, r32 B)
{
    A.X *= B;
    A.Y *= B;
    A.Z *= B;

    return(A);
}

function v3
Hadamard(v3 A, v3 B)
{
    v3 Result;

    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;

    return(Result);
}

function r32
Inner(v3 A, v3 B)
{
    r32 Result = A.X*B.X + A.Y*B.Y + A.Z*B.Z;
    return(Result);
}

function r32
LengthSq(v3 A)
{
    r32 Result = Inner(A, A);
    return(Result);
}

function r32
Length(v3 A)
{
    r32 Result = SquareRoot(LengthSq(A));
    return(Result);
}

function v3
Clamp01(v3 A)
{
    v3 Result;

    Result.X = Clamp01(A.X);
    Result.Y = Clamp01(A.Y);
    Result.Z = Clamp01(A.Z);

    return(Result);
}

union v4
{
    struct
    {
        r32 X;
        r32 Y;
        r32 Z;
        r32 W;
    };
    struct
    {
        v3 XYZ;
        r32 W;
    };
    struct
    {
        r32 R;
        r32 G;
        r32 B;
        r32 A;
    };
    struct
    {
        v3 RGB;
        r32 A;
    };
    r32 E[4];
};

function v4
V4(r32 X, r32 Y, r32 Z, r32 W)
{
    v4 Result;

    Result.X = X;
    Result.Y = Y;
    Result.Z = Z;
    Result.W = W;

    return(Result);
}

function v4
ToV4(v3 XYZ, r32 W)
{
    v4 Result;

    Result.XYZ = XYZ;
    Result.W = W;

    return(Result);
}

function v4
operator-(v4 A)
{
    v4 Result;

    Result.X = -A.X;
    Result.Y = -A.Y;
    Result.Z = -A.Z;
    Result.W = -A.W;

    return(Result);
}

function v4
operator+(v4 A, v4 B)
{
    v4 Result;

    Result.X = A.X + B.X;
    Result.Y = A.Y + B.Y;
    Result.Z = A.Z + B.Z;
    Result.W = A.W + B.W;

    return(Result);
}

function v4
operator-(v4 A, v4 B)
{
    v4 Result;

    Result.X = A.X - B.X;
    Result.Y = A.Y - B.Y;
    Result.Z = A.Z - B.Z;
    Result.W = A.W - B.W;

    return(Result);
}

function v4
operator*(v4 A, r32 B)
{
    v4 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;
    Result.Z = A.Z * B;
    Result.W = A.W * B;

    return(Result);
}

function v4
operator*(r32 B, v4 A)
{
    v4 Result;

    Result.X = A.X * B;
    Result.Y = A.Y * B;
    Result.Z = A.Z * B;
    Result.W = A.W * B;

    return(Result);
}

function v4
operator+=(v4 &A, v4 B)
{
    A.X += B.X;
    A.Y += B.Y;
    A.Z += B.Z;
    A.W += B.W;

    return(A);
}

function v4
operator-=(v4 &A, v4 B)
{
    A.X -= B.X;
    A.Y -= B.Y;
    A.Z -= B.Z;
    A.W -= B.W;

    return(A);
}

function v4
operator*=(v4 &A, r32 B)
{
    A.X *= B;
    A.Y *= B;
    A.Z *= B;
    A.W *= B;

    return(A);
}

function v4
Hadamard(v4 A, v4 B)
{
    v4 Result;

    Result.X = A.X * B.X;
    Result.Y = A.Y * B.Y;
    Result.Z = A.Z * B.Z;
    Result.W = A.W * B.W;

    return(Result);
}

function r32
Inner(v4 A, v4 B)
{
    r32 Result = A.X*B.X + A.Y*B.Y + A.Z*B.Z + A.W*B.W;
    return(Result);
}

function r32
LengthSq(v4 A)
{
    r32 Result = Inner(A, A);
    return(Result);
}

function r32
Length(v4 A)
{
    r32 Result = SquareRoot(LengthSq(A));
    return(Result);
}

function v4
Clamp01(v4 A)
{
    v4 Result;

    Result.X = Clamp01(A.X);
    Result.Y = Clamp01(A.Y);
    Result.Z = Clamp01(A.Z);
    Result.W = Clamp01(A.W);

    return(Result);
}

function v4
Lerp(v4 A, r32 t, v4 B)
{
    v4 Result = (1.0f - t)*A + t*B;
    return(Result);
}

function v4
Normalize(v4 A)
{
    v4 Result = A * (1.0f / LengthSq(A));

    return(Result);
}

struct rectangle2
{
    v2 Min;
    v2 Max;
};

function v2
GetMinCorner(rectangle2 Rect)
{
    v2 Result = Rect.Min;
    return(Result);
}

function v2
GetMaxCorner(rectangle2 Rect)
{
    v2 Result = Rect.Max;
    return(Result);
}

function v2
GetCenter(rectangle2 Rect)
{
    v2 Result = 0.5f*(Rect.Min + Rect.Max);
    return(Result);
}

function v2
GetDim(rectangle2 Rect)
{
    v2 Result = Rect.Max - Rect.Min;
    return(Result);
}

function rectangle2
RectMinMax(v2 Min, v2 Max)
{
    rectangle2 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

function rectangle2
RectMinDim(v2 Min, v2 Dim)
{
    rectangle2 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

function rectangle2
RectCenterHalfDim(v2 Center, v2 HalfDim)
{
    rectangle2 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

function rectangle2
RectCenterDim(v2 Center, v2 Dim)
{
    rectangle2 Result;

    Result.Min = Center - 0.5f*Dim;
    Result.Max = Center + 0.5f*Dim;

    return(Result);
}

function rectangle2
AddRadiusTo(rectangle2 Rectangle, v2 Radius)
{
    rectangle2 Result = Rectangle;

    Result.Min -= Radius;
    Result.Max += Radius;

    return(Result);
}

function rectangle2
Offset(rectangle2 A, v2 Offset)
{
    rectangle2 Result = A;

    A.Min += Offset;
    A.Max += Offset;

    return(Result);
}

function b32
IsInRectangle(rectangle2 Rectangle, v2 Test)
{
    b32 Result = ((Test.X >= Rectangle.Min.X) &&
                  (Test.Y >= Rectangle.Min.Y) &&
                  (Test.X < Rectangle.Max.X) &&
                  (Test.Y < Rectangle.Max.Y));
    return(Result);
}

function b32
RectanglesIntersect(rectangle2 A, rectangle2 B)
{
    b32 Result = !((B.Max.X <= A.Min.X) ||
                   (B.Min.X >= A.Max.X) ||
                   (B.Max.Y <= A.Min.Y) ||
                   (B.Min.Y >= A.Max.Y));
    return(Result);
}

function v2
GetBarycentric(rectangle2 A, v2 P)
{
    v2 Result;

    Result.X = SafeRatio0(P.X - A.Min.X, A.Max.X - A.Min.X);
    Result.Y = SafeRatio0(P.Y - A.Min.Y, A.Max.Y - A.Min.Y);

    return(Result);
}

struct rectangle3
{
    v3 Min;
    v3 Max;
};

function v3
GetMinCorner(rectangle3 Rect)
{
    v3 Result = Rect.Min;
    return(Result);
}

function v3
GetMaxCorner(rectangle3 Rect)
{
    v3 Result = Rect.Max;
    return(Result);
}

function v3
GetCenter(rectangle3 Rect)
{
    v3 Result = 0.5f*(Rect.Min + Rect.Max);
    return(Result);
}

function rectangle3
RectMinMax(v3 Min, v3 Max)
{
    rectangle3 Result;

    Result.Min = Min;
    Result.Max = Max;

    return(Result);
}

function rectangle3
RectMinDim(v3 Min, v3 Dim)
{
    rectangle3 Result;

    Result.Min = Min;
    Result.Max = Min + Dim;

    return(Result);
}

function rectangle3
RectCenterHalfDim(v3 Center, v3 HalfDim)
{
    rectangle3 Result;

    Result.Min = Center - HalfDim;
    Result.Max = Center + HalfDim;

    return(Result);
}

function rectangle3
RectCenterDim(v3 Center, v3 Dim)
{
    rectangle3 Result;

    Result.Min = Center - 0.5f*Dim;
    Result.Max = Center + 0.5f*Dim;

    return(Result);
}

function rectangle3
AddRadiusTo(rectangle3 Rectangle, v3 Radius)
{
    rectangle3 Result = Rectangle;

    Result.Min -= Radius;
    Result.Max += Radius;

    return(Result);
}

function rectangle3
Offset(rectangle3 A, v3 Offset)
{
    rectangle3 Result = A;

    A.Min += Offset;
    A.Max += Offset;

    return(Result);
}

function b32
IsInRectangle(rectangle3 Rectangle, v3 Test)
{
    b32 Result = ((Test.X >= Rectangle.Min.X) &&
                  (Test.Y >= Rectangle.Min.Y) &&
                  (Test.Z >= Rectangle.Min.Z) &&
                  (Test.X < Rectangle.Max.X) &&
                  (Test.Y < Rectangle.Max.Y) &&
                  (Test.Z < Rectangle.Max.Z));
    return(Result);
}

function b32
RectanglesIntersect(rectangle3 A, rectangle3 B)
{
    b32 Result = !((B.Max.X <= A.Min.X) ||
                   (B.Min.X >= A.Max.X) ||
                   (B.Max.Y <= A.Min.Y) ||
                   (B.Min.Y >= A.Max.Y) ||
                   (B.Max.Z <= A.Min.Z) ||
                   (B.Min.Z >= A.Max.Z));
    return(Result);
}

function v3
GetBarycentric(rectangle3 A, v3 P)
{
    v3 Result;

    Result.X = SafeRatio0(P.X - A.Min.X, A.Max.X - A.Min.X);
    Result.Y = SafeRatio0(P.Y - A.Min.Y, A.Max.Y - A.Min.Y);
    Result.Z = SafeRatio0(P.Z - A.Min.Z, A.Max.Z - A.Min.Z);

    return(Result);
}

function rectangle2
ToRectangleXY(rectangle3 A)
{
    rectangle2 Result;

    Result.Min = A.Min.XY;
    Result.Max = A.Max.XY;

    return(Result);
}

struct rectangle2i
{
    s32 MinX;
    s32 MinY;
    s32 MaxX;
    s32 MaxY;
};

function rectangle2i
Intersect(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;

    Result.MinX = (A.MinX < B.MinX) ? B.MinX : A.MinX;
    Result.MinY = (A.MinY < B.MinY) ? B.MinY : A.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? B.MaxX : A.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? B.MaxY : A.MaxY;

    return(Result);
}

function rectangle2i
Union(rectangle2i A, rectangle2i B)
{
    rectangle2i Result;

    Result.MinX = (A.MinX < B.MinX) ? A.MinX : B.MinX;
    Result.MinY = (A.MinY < B.MinY) ? A.MinY : B.MinY;
    Result.MaxX = (A.MaxX > B.MaxX) ? A.MaxX : B.MaxX;
    Result.MaxY = (A.MaxY > B.MaxY) ? A.MaxY : A.MaxY;

    return(Result);
}

function rectangle2i
InvertedInfinityRectangle(void)
{
    rectangle2i Result;

    Result.MinX = Result.MinY = S32Max;
    Result.MaxX = Result.MaxY = S32Min;

    return(Result);
}

function v4
SRGB255ToLinear1(v4 C)
{
    v4 Result = C;

    Result *= 1.0f/255;

    Result.A = Result.A;
    Result.R = Square(Result.R);
    Result.G = Square(Result.G);
    Result.B = Square(Result.B);

    return(Result);
}

function v4
Linear1ToSRGB255(v4 C)
{
    v4 Result = C;

    Result.A = Result.A;
    Result.R = SquareRoot(Result.R);
    Result.G = SquareRoot(Result.G);
    Result.B = SquareRoot(Result.B);

    Result *= 255.0f;

    return(Result);
}
