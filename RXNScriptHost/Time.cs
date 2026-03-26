
namespace RXNEngine
{
    public static class Time
    {
        public static float DeltaTime { get; internal set; }

        public static float TimeScale { get; set; } = 1.0f;

        public static float TimeSinceStartup { get; internal set; }
    }
}
