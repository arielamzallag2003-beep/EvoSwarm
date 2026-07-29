using System;

namespace PalaceOfFantasy.FlockForge.Core
{
    public readonly struct FVector3 : IEquatable<FVector3>
    {
        public readonly float X;
        public readonly float Y;
        public readonly float Z;

        public FVector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static FVector3 Zero => new(0, 0, 0);
        public static FVector3 One => new(1, 1, 1);
        public static FVector3 Up => new(0, 1, 0);
        public static FVector3 Down => new(0, -1, 0);
        public static FVector3 Left => new(-1, 0, 0);
        public static FVector3 Right => new(1, 0, 0);
        public static FVector3 Forward => new(0, 0, 1);
        public static FVector3 Back => new(0, 0, -1);

        public float SqrMagnitude => X * X + Y * Y + Z * Z;
        public float Magnitude => MathF.Sqrt(SqrMagnitude);

        public FVector3 Normalized
        {
            get
            {
                float mag = Magnitude;
                return mag > 1e-5f ? this / mag : Zero;
            }
        }

        public static FVector3 operator +(FVector3 a, FVector3 b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        public static FVector3 operator -(FVector3 a, FVector3 b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        public static FVector3 operator -(FVector3 v) => new(-v.X, -v.Y, -v.Z);
        public static FVector3 operator *(FVector3 v, float s) => new(v.X * s, v.Y * s, v.Z * s);
        public static FVector3 operator *(float s, FVector3 v) => new(v.X * s, v.Y * s, v.Z * s);
        public static FVector3 operator /(FVector3 v, float s) => new(v.X / s, v.Y / s, v.Z / s);

        public static float Dot(FVector3 a, FVector3 b) => a.X * b.X + a.Y * b.Y + a.Z * b.Z;

        public static FVector3 Cross(FVector3 a, FVector3 b) => new(
            a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X
        );

        public static float Distance(FVector3 a, FVector3 b) => (a - b).Magnitude;
        public static float SqrDistance(FVector3 a, FVector3 b) => (a - b).SqrMagnitude;

        public static FVector3 Lerp(FVector3 a, FVector3 b, float t)
        {
            t = FMath.Clamp01(t);
            return new(a.X + (b.X - a.X) * t, a.Y + (b.Y - a.Y) * t, a.Z + (b.Z - a.Z) * t);
        }

        public static FVector3 ClampMagnitude(FVector3 v, float maxMagnitude)
        {
            float sqrMag = v.SqrMagnitude;
            if (sqrMag > maxMagnitude * maxMagnitude)
            {
                float mag = MathF.Sqrt(sqrMag);
                return v / mag * maxMagnitude;
            }
            return v;
        }

        public bool Equals(FVector3 other) => X == other.X && Y == other.Y && Z == other.Z;
        public override bool Equals(object obj) => obj is FVector3 other && Equals(other);
        public override int GetHashCode() => HashCode.Combine(X, Y, Z);
        public static bool operator ==(FVector3 a, FVector3 b) => a.Equals(b);
        public static bool operator !=(FVector3 a, FVector3 b) => !a.Equals(b);

        public override string ToString() => $"({X:F2}, {Y:F2}, {Z:F2})";
    }
}
