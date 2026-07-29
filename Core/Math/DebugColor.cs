namespace PalaceOfFantasy.FlockForge.Core
{
    public readonly struct DebugColor
    {
        public readonly float R, G, B, A;

        public DebugColor(float r, float g, float b, float a = 1f)
        {
            R = r;
            G = g;
            B = b;
            A = a;
        }

        public static DebugColor Red => new(1, 0, 0);
        public static DebugColor Green => new(0, 1, 0);
        public static DebugColor Blue => new(0, 0, 1);
        public static DebugColor Yellow => new(1, 1, 0);
        public static DebugColor Cyan => new(0, 1, 1);
        public static DebugColor Magenta => new(1, 0, 1);
        public static DebugColor White => new(1, 1, 1);
        public static DebugColor Black => new(0, 0, 0);
        public static DebugColor Orange => new(1, 0.5f, 0);
    }
}
