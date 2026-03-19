using System;
using RXNEngine;


public class Player : Entity
{
    public float Speed = 5.0f;
    public float MouseSensitivity = 0.002f;
    public float JumpForce = 1.0f;
    public float BulletForce = 0.1f;
    public int Ammo = 1000;
    public bool IsHovering = false;
    public Vector3 SpawnOffset = new Vector3(0, 0.5f, 0);
    public Vector4 PlayerColor = new Vector4(0.2f, 0.8f, 0.3f, 1.0f);
    public Entity? BulletPrefab;
    public Entity? HeadCamera;

    private bool m_JumpRequested = false;
    private int m_CurrentContacts = 0;

    private Vector2 m_LastMousePos;
    private float m_Pitch = 0.0f;
    private float m_Yaw = 0.0f;

    private bool m_IsLocked = true;

    public override void OnCreate()
    {
        Input.SetCursorMode(CursorMode.Locked);
        m_LastMousePos = Input.GetMousePosition();

        Vector3 currentRot = this.Rotation;
        m_Pitch = currentRot.X;
        m_Yaw = currentRot.Y;

        HeadCamera = FindEntityByName("Camera");
    }

    public override void OnUpdate(float deltaTime)
    {
        Vector2 currentMousePos = Input.GetMousePosition();
        Vector2 delta = currentMousePos - m_LastMousePos;
        m_LastMousePos = currentMousePos;

        m_Yaw -= delta.X * MouseSensitivity;
        m_Pitch -= delta.Y * MouseSensitivity;

        m_Pitch = Math.Clamp(m_Pitch, -1.5f, 1.5f);

        this.Rotation = new Vector3(0.0f, m_Yaw, 0.0f);

        if (HeadCamera != null)
            HeadCamera.Rotation = new Vector3(m_Pitch, 0.0f, 0.0f);


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

        if (Input.IsKeyDown(KeyCode.F) && Ammo > 0)
        {
            /*
             if(BulletPrefab != null)
             {
                Entity firedBullet = Entity.Instantiate(BulletPrefab);

                firedBullet.Translation = pos + (HeadCamera.Forward * 1.5f) + SpawnOffset;

                firedBullet.ApplyLinearImpulse(HeadCamera.Forward * BulletForce);

                Ammo--;
                Console.WriteLine($"[Player] Fired! Ammo left: {Ammo}. Bullet ID: {firedBullet.ID}");
             }
            */

            Vector3 origin = HeadCamera != null ? HeadCamera.WorldPosition : this.WorldPosition;
            Vector3 dir = HeadCamera != null ? HeadCamera.Forward : forward;

            if (Physics.Raycast(origin, dir, 1000.0f, out RaycastHit hit, this))
            {
                Console.WriteLine($"[Hitscan] POW! Hit Entity {hit.EntityID} at Distance: {hit.Distance}");
                Console.WriteLine($"[Hitscan] Hit Coordinate: {hit.Position}");

                Entity? target = Entity.FindEntityByName("Enemy");
                if (target != null && target.ID == hit.EntityID)
                {
                    target.ApplyLinearImpulse(dir * 50.0f);
                }
            }
            else
            {
                Console.WriteLine("[Hitscan] Missed! Shot went into the void.");
            }
        }

        if (Input.IsKeyDown(KeyCode.X))
        {
            Console.WriteLine("[Player] X pressed. Initiating Self-Destruct...");
            this.Destroy();
            return;
        }

        if (Input.IsKeyDown(KeyCode.Escape))
        {
            if(m_IsLocked)
            {
                Input.SetCursorMode(CursorMode.Normal);
                m_IsLocked = false;
            }
            else
            {
                Input.SetCursorMode(CursorMode.Locked);
                m_IsLocked = true;
            }
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