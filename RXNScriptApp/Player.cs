using System;
using RXNEngine;

public class Player : Entity
{
    public float Speed = 5.0f;
    public Entity? m_BulletPrefab;

    public bool IsInvincible = false;
    public int Amount = 0;
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

        Vector3 forward = this.Forward;
        Vector3 right = this.Right;
        Vector3 up = this.Up;

        if (Input.IsKeyDown(KeyCode.W))
            velocity += forward;
        if (Input.IsKeyDown(KeyCode.S))
            velocity -= forward;
        if (Input.IsKeyDown(KeyCode.D))
            velocity += right;
        if (Input.IsKeyDown(KeyCode.A))
            velocity -= right;
        if (Input.IsKeyDown(KeyCode.Q))
            velocity += up;
        if (Input.IsKeyDown(KeyCode.E))
            velocity -= up;

        if (Input.IsKeyDown(KeyCode.Space))
        {
            this.ApplyLinearImpulse(up * 1.0f);
        }
        if (Input.IsKeyDown(KeyCode.F) && m_BulletPrefab != null)
        {
            Entity firedBullet = Entity.Instantiate(m_BulletPrefab);
            firedBullet.Translation = pos + (forward * 1.0f) + SpawnOffset;
            firedBullet.ApplyLinearImpulse(forward * 0.1f);

            Amount++;
        }

        pos += velocity * Speed * deltaTime;
        Translation = pos;
    }
}