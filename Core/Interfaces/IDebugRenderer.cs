namespace PalaceOfFantasy.FlockForge.Core
{
    public interface IDebugRenderer
    {
        bool IsEnabled { get; set; }

        void DrawLine(FVector3 from, FVector3 to, DebugColor color);
        void DrawSphere(FVector3 center, float radius, DebugColor color);
        void DrawCone(FVector3 origin, FVector3 direction, float angle, float length, DebugColor color);
        void DrawText(FVector3 position, string text, DebugColor color);
    }
}
