using System;

namespace RXNEngine
{
    public class Player : Entity
    {
        public float Speed = 5.0f;

        public override void OnCreate()
        {
            Console.WriteLine($"Player Script Spawned!");
        }

        public override void OnUpdate(float deltaTime)
        {
            Vector3 pos = Translation;
            Vector3 velocity = new Vector3(0, 0, 0);

            if (Input.IsKeyDown(KeyCode.W))
                velocity.Y += 1.0f;
            if (Input.IsKeyDown(KeyCode.S))
                velocity.Y -= 1.0f;
            if (Input.IsKeyDown(KeyCode.A))
                velocity.X -= 1.0f;
            if (Input.IsKeyDown(KeyCode.D))
                velocity.X += 1.0f;

            pos.X += velocity.X * Speed * deltaTime;
            pos.Y += velocity.Y * Speed * deltaTime;

            Translation = pos;
        }
    }
}