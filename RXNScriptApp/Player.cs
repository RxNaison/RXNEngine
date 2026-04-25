using RXNEngine;
using System;
using System.Threading;
using System.Collections;

public class Player : Entity
{
    public int BulletsShot = 0;
    public float Speed = 5.0f;

    public float MouseSensitivity = 0.002f;
    public float GamepadLookSensitivity = 2.5f;

    public float JumpForce = 7.0f;
    public float BulletForce = 0.1f;
    public int Ammo = 1000;
    public bool IsHovering = false;
    public Vector3 SpawnOffset = new Vector3(0, 0.5f, 0);
    public Vector4 PlayerColor = new Vector4(0.2f, 0.8f, 0.3f, 1.0f);
    public Entity? BulletPrefab;
    public Entity? HeadCamera;
    public Entity? SunEntity;

    private float m_FireCooldown = 0.0f;
    private int m_CurrentContacts = 0;

    private float m_Pitch = 0.0f;
    private float m_Yaw = 0.0f;
    private bool m_IsLocked = true;

    private float m_VerticalVelocity = 0.0f;
    private float m_Gravity = -19.62f;
    private bool m_IsGrounded = false;

    public override void OnCreate()
    {
        Input.SetCursorMode(CursorMode.Locked);

        Vector3 currentRot = this.Rotation;
        m_Pitch = currentRot.X;
        m_Yaw = currentRot.Y;

        HeadCamera = FindEntityByName("Camera");

        Console.WriteLine("\n=== RXN ENGINE: COMPONENT BRIDGE TEST ===");

        if (this.HasComponent<TagComponent>())
        {
            var tagComp = this.GetComponent<TagComponent>();
            Console.WriteLine($"[Tag Test] Original Name: {tagComp!.Tag}");

            tagComp.Tag = "DoomSlayer";
            Console.WriteLine($"[Tag Test] Renamed to: {tagComp.Tag}");
        }

        if (this.HasComponent<ScriptComponent>())
        {
            var scriptComp = this.GetComponent<ScriptComponent>();
            Console.WriteLine($"[Script Test] Running Class: {scriptComp!.ScriptPath}");
        }

        if (this.HasComponent<RelationshipComponent>())
        {
            var relComp = this.GetComponent<RelationshipComponent>();
            Entity? myParent = relComp!.Parent;

            if (myParent != null)
                Console.WriteLine($"[Hierarchy Test] My Parent's ID is: {myParent.ID}");
            else
                Console.WriteLine("[Hierarchy Test] I am an orphan (No Parent).");
        }

        Console.WriteLine("=========================================\n");
    }

    public override void OnUpdate(float deltaTime)
    {
        Vector2 delta = Input.GetMouseDelta();

        m_Yaw -= delta.X * MouseSensitivity;
        m_Pitch -= delta.Y * MouseSensitivity;

        float rightX = Input.GetGamepadAxis(0, GamepadAxis.RightX);
        float rightY = Input.GetGamepadAxis(0, GamepadAxis.RightY);

        m_Yaw -= rightX * GamepadLookSensitivity * deltaTime;
        m_Pitch -= rightY * GamepadLookSensitivity * deltaTime;

        m_Pitch = Math.Clamp(m_Pitch, -1.5f, 1.5f);

        this.Rotation = new Vector3(0.0f, m_Yaw, 0.0f);

        if (HeadCamera != null)
            HeadCamera.Rotation = new Vector3(m_Pitch, 0.0f, 0.0f);


        Vector3 velocity = new Vector3(0, 0, 0);

        Vector3 forward = this.Forward;
        Vector3 right = this.Right;

        forward.Y = 0;
        right.Y = 0;

        if (Input.IsKeyDown(KeyCode.W)) velocity += forward;
        if (Input.IsKeyDown(KeyCode.S)) velocity -= forward;
        if (Input.IsKeyDown(KeyCode.D)) velocity += right;
        if (Input.IsKeyDown(KeyCode.A)) velocity -= right;

        float leftX = Input.GetGamepadAxis(0, GamepadAxis.LeftX);
        float leftY = Input.GetGamepadAxis(0, GamepadAxis.LeftY);

        velocity += right * leftX;
        velocity -= forward * leftY;

        m_VerticalVelocity += m_Gravity * deltaTime;

        if ((Input.IsKeyDown(KeyCode.Space) || Input.IsGamepadButtonDown(0, GamepadButton.A)) && m_IsGrounded)
        {
            m_VerticalVelocity = JumpForce;
            m_IsGrounded = false;
        }

        if (IsHovering) m_VerticalVelocity = 0.0f;

        if (HasComponent<CharacterControllerComponent>())
        {
            var cct = GetComponent<CharacterControllerComponent>();

            Vector3 displacement = (velocity * Speed * deltaTime) + new Vector3(0, m_VerticalVelocity * deltaTime, 0);

            CollisionFlags flags = cct!.Move(displacement, deltaTime);

            m_IsGrounded = (flags & CollisionFlags.Down) != 0;

            if (m_IsGrounded && m_VerticalVelocity < 0.0f)
                m_VerticalVelocity = -1.0f;
            else if ((flags & CollisionFlags.Up) != 0 && m_VerticalVelocity > 0.0f)
                m_VerticalVelocity = -1.0f;
        }


        bool rightTriggerPulled = Input.GetGamepadAxis(0, GamepadAxis.RightTrigger) > 0.5f;
        bool fireIntent = Input.IsKeyDown(KeyCode.F) || rightTriggerPulled;

        m_FireCooldown -= deltaTime;

        if (fireIntent && Ammo > 0 && m_FireCooldown <= 0.0f)
        {
            if (BulletPrefab != null && HeadCamera != null)
            {
                m_FireCooldown = 0.2f;
                StartCoroutine(FireWeaponRoutine(Translation));
            }
        }

        if (Input.IsKeyDown(KeyCode.M))
        {
            Entity newProp = Entity.Instantiate();
            newProp.Translation = this.Translation + (this.Forward * 5.0f);
            AssetManager.LoadMeshOntoEntityAsync("assets/models/porsche/scene.gltf", newProp);
        }

        if (Input.IsKeyDown(KeyCode.X))
        {
            Console.WriteLine("[Player] X pressed. Initiating Self-Destruct...");
            this.Destroy();
            return;
        }

        if (Input.IsKeyDown(KeyCode.Escape) || Input.IsGamepadButtonDown(0, GamepadButton.Start))
        {
            if (m_IsLocked)
            {
                Input.SetCursorMode(CursorMode.Normal);
                m_IsLocked = false;
            }
            else
            {
                Input.SetCursorMode(CursorMode.Locked);
                m_IsLocked = true;
            }
            Thread.Sleep(50); 
        }
    }

    public override void OnFixedUpdate(float fixedTimeStep)
    {
    }

    private IEnumerator FireWeaponRoutine(Vector3 startPos)
    {
        Entity firedBullet = Entity.Instantiate(BulletPrefab!);
        firedBullet.Translation = startPos + (HeadCamera!.Forward * 1.5f) + SpawnOffset;

        firedBullet.ApplyLinearImpulse(HeadCamera.Forward * BulletForce);

        Ammo--;
        BulletsShot++;
        Console.WriteLine($"[Player] Fired! Ammo left: {Ammo}. Bullet ID: {firedBullet.ID}");

        Input.SetGamepadVibration(0, 1.0f, 1.0f);

        yield return new WaitForSeconds(0.15f);

        Input.SetGamepadVibration(0, 0.0f, 0.0f);
    }

    public override void OnCollisionEnter(Entity other)
    {
        m_CurrentContacts++;
    }

    public override void OnCollisionExit(Entity other)
    {
        m_CurrentContacts--;
    }

    public override void OnTriggerEnter(Entity other)
    {
        var trigger = FindEntityByName("ReloadAmmoPlace");

        if (trigger != null && other.ID == trigger.ID)
            Ammo = 1000;

        Console.WriteLine($"[SENSOR] Player walked into a trigger zone! Entity ID: {other.ID}");
    }

    public override void OnDestroy()
    {
        Console.WriteLine($"[Player] OnDestroy triggered! Memory cleaned up. Goodbye!");
    }
}