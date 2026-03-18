using System;
using RXNEngine;


public class Player : Entity
{
    public float Speed = 5.0f;
    public float JumpForce = 1.0f;
    public float BulletForce = 0.1f;
    public int Ammo = 1000;
    public bool IsHovering = false;
    public Vector3 SpawnOffset = new Vector3(0, 0.5f, 0);
    public Vector4 PlayerColor = new Vector4(0.2f, 0.8f, 0.3f, 1.0f);
    public Entity? BulletPrefab;

    private bool m_JumpRequested = false;
    private int m_CurrentContacts = 0;

    public override void OnCreate()
    {
        Console.WriteLine($"[Player] OnCreate: Spawned successfully with Entity ID {ID}!");
    }

    public override void OnUpdate(float deltaTime)
    {
        Vector3 pos = Translation;
        Vector3 velocity = new Vector3(0, 0, 0);

        Vector3 forward = this.Forward;
        Vector3 right = this.Right;
        Vector3 up = this.Up;

        if (Input.IsKeyDown(KeyCode.W)) velocity += forward;
        if (Input.IsKeyDown(KeyCode.S)) velocity -= forward;
        if (Input.IsKeyDown(KeyCode.D)) velocity += right;
        if (Input.IsKeyDown(KeyCode.A)) velocity -= right;

        if (Input.IsKeyDown(KeyCode.Space))
            m_JumpRequested = true;

        if (Input.IsKeyDown(KeyCode.F) && BulletPrefab != null && Ammo > 0)
        {
            Entity firedBullet = Entity.Instantiate(BulletPrefab);

            firedBullet.Translation = pos + (forward * 1.5f) + SpawnOffset;

            firedBullet.ApplyLinearImpulse(forward * BulletForce);

            Ammo--;
            Console.WriteLine($"[Player] Fired! Ammo left: {Ammo}. Bullet ID: {firedBullet.ID}");
        }

        if (Input.IsKeyDown(KeyCode.X))
        {
            Console.WriteLine("[Player] X pressed. Initiating Self-Destruct...");
            this.Destroy();
            return;
        }

        if (velocity.X != 0 || velocity.Y != 0 || velocity.Z != 0)
        {
            pos += velocity * Speed * deltaTime;
            Translation = pos;
        }
    }

    public override void OnFixedUpdate(float fixedTimeStep)
    {
        if (m_JumpRequested)
        {
            this.ApplyLinearImpulse(this.Up * JumpForce);
            m_JumpRequested = false;
            Console.WriteLine("[Player] Jump Impulse Applied!");
        }

        if (IsHovering)
        {
            this.ApplyLinearImpulse(new Vector3(0, 9.81f * fixedTimeStep, 0));
        }
    }

    public override void OnCollisionEnter(Entity other)
    {
        m_CurrentContacts++;
        Console.WriteLine($"[Player] Collision ENTER with Entity {other.ID}. (Total touching: {m_CurrentContacts})");
    }

    public override void OnCollisionExit(Entity other)
    {
        m_CurrentContacts--;
        Console.WriteLine($"[Player] Collision EXIT with Entity {other.ID}.");
    }

    public override void OnDestroy()
    {
        Console.WriteLine($"[Player] OnDestroy triggered! Memory cleaned up. Goodbye!");
    }
}