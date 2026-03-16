using System;
using RXNEngine;

public class Player : Entity
{
    public float Speed = 5.0f;
    public float Gap = 0.0f;
    private Entity? m_BulletPrefab;

    public override void OnCreate()
    {
        m_BulletPrefab = Entity.FindEntityByName("BulletTemplate");

        if (m_BulletPrefab == null)
            Console.WriteLine("ERROR: Could not find 'BulletTemplate' in the scene!");
    }

    public override void OnUpdate(float deltaTime)
    {
        Vector3 pos = Translation;
        Vector3 velocity = new Vector3(0, 0, 0);

        if (Input.IsKeyDown(KeyCode.W))
            velocity.Z -= 1.0f;
        if (Input.IsKeyDown(KeyCode.S))
            velocity.Z += 1.0f;
        if (Input.IsKeyDown(KeyCode.A))
            velocity.X -= 1.0f;
        if (Input.IsKeyDown(KeyCode.D))
            velocity.X += 1.0f;
        if (Input.IsKeyDown(KeyCode.Q))
            velocity.Y += 1.0f;
        if (Input.IsKeyDown(KeyCode.E))
            velocity.Y -= 1.0f;
        if (Input.IsKeyDown(KeyCode.Space) && m_BulletPrefab != null)
        {
            Entity firedBullet = Entity.Instantiate(m_BulletPrefab);
            firedBullet.Translation = firedBullet.Translation + new Vector3(Gap, 0, 0);
            Gap += 3.5f;
        }

        pos.X += velocity.X * Speed * deltaTime;
        pos.Y += velocity.Y * Speed * deltaTime;
        pos.Z += velocity.Z * Speed * deltaTime;

        Translation = pos;
    }
}