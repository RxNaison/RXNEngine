using System;
using RXNEngine;

public class Player : Entity
{
    public float Speed = 5.0f;
    public Entity? m_BulletPrefab;

    public bool IsInvincible = false;
    public int Ammo = 30;
    public Vector3 SpawnOffset = new Vector3(0, 0.0f, 0);
    public Vector4 LaserColor = new Vector4(1.0f, 0.0f, 0.0f, 1.0f);
    public Vector4 Laser = new Vector4(1.0f, 0.0f, 0.0f, 1.0f);

    public override void OnCreate()
    {
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
        if (Input.IsKeyDown(KeyCode.Space))
        {
            Vector3 jumpForce = new Vector3(0.0f, 1.0f, 0.0f);
            this.ApplyLinearImpulse(jumpForce);
        }
        if (Input.IsKeyDown(KeyCode.F) && m_BulletPrefab != null)
        {
            Entity firedBullet = Entity.Instantiate(m_BulletPrefab);
            firedBullet.Translation = new Vector3(pos.X, pos.Y, pos.Z - 1.0f) + SpawnOffset;
            firedBullet.ApplyLinearImpulse(new Vector3(0.0f, 0.0f, -0.1f));
        }

        pos.X += velocity.X * Speed * deltaTime;
        pos.Y += velocity.Y * Speed * deltaTime;
        pos.Z += velocity.Z * Speed * deltaTime;

        Translation = pos;
    }
}