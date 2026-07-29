using System;

namespace PalaceOfFantasy.FlockForge.Core
{
    public static class FMath
    {
        public const float PI = 3.14159265359f;
        public const float Deg2Rad = PI / 180f;
        public const float Rad2Deg = 180f / PI;
        public const float Epsilon = 1e-5f;

        public static float Clamp(float value, float min, float max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        public static float Clamp01(float value) => Clamp(value, 0f, 1f);

        public static float Lerp(float a, float b, float t) => a + (b - a) * Clamp01(t);

        public static float InverseLerp(float a, float b, float value)
        {
            if (MathF.Abs(b - a) < Epsilon) return 0f;
            return Clamp01((value - a) / (b - a));
        }

        public static float Min(float a, float b) => a < b ? a : b;
        public static float Max(float a, float b) => a > b ? a : b;
        public static float Abs(float value) => MathF.Abs(value);
        public static float Sign(float value) => value >= 0 ? 1f : -1f;

        public static float MoveTowards(float current, float target, float maxDelta)
        {
            if (Abs(target - current) <= maxDelta) return target;
            return current + Sign(target - current) * maxDelta;
        }

        public static float SmoothDamp(float current, float target, ref float velocity, float smoothTime, float deltaTime)
        {
            smoothTime = Max(0.0001f, smoothTime);
            float omega = 2f / smoothTime;
            float x = omega * deltaTime;
            float exp = 1f / (1f + x + 0.48f * x * x + 0.235f * x * x * x);
            float change = current - target;
            float temp = (velocity + omega * change) * deltaTime;
            velocity = (velocity - omega * temp) * exp;
            return target + (change + temp) * exp;
        }
    }
}
