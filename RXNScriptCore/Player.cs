using System;

namespace RXNEngine
{
    public class Player : Entity
    {
        public override void OnCreate()
        {
            Console.WriteLine($"Player Script Spawned! ID: {ID}");
        }

        public override void OnUpdate(float deltaTime)
        {
            Vector3 pos = Translation;
            pos.Y += 1.0f * deltaTime;
            Translation = pos;
        }
    }
}