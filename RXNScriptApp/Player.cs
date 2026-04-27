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
    private Vector3 m_CurrentVelocity = new Vector3(0, 0, 0);
    private float m_Gravity = -19.62f;
    private bool m_IsGrounded = false;

    private bool m_IsVideoPlaying = false;

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


        Vector3 targetVelocity = new Vector3(0, 0, 0);
        Vector3 forward = this.Forward;
        Vector3 right = this.Right;
        forward.Y = 0; right.Y = 0;

        if (Input.IsKeyDown(KeyCode.W)) targetVelocity += forward;
        if (Input.IsKeyDown(KeyCode.S)) targetVelocity -= forward;
        if (Input.IsKeyDown(KeyCode.D)) targetVelocity += right;
        if (Input.IsKeyDown(KeyCode.A)) targetVelocity -= right;

        float leftX = Input.GetGamepadAxis(0, GamepadAxis.LeftX);
        float leftY = Input.GetGamepadAxis(0, GamepadAxis.LeftY);
        targetVelocity += right * leftX;
        targetVelocity -= forward * leftY;

        float dynFriction = 0.5f;
        float staticFriction = 0.5f;
        float restitution = 0.1f;
        Vector3 groundNormal = new Vector3(0, 1, 0);

        if (Physics.Raycast(this.Translation, new Vector3(0, -1, 0), 1.5f, out RaycastHit hit, this))
        {
            dynFriction = hit.DynamicFriction;
            staticFriction = hit.StaticFriction;
            restitution = hit.Restitution;
            groundNormal = hit.Normal;
        }

        bool isStandingStill = (Math.Abs(m_CurrentVelocity.X) < 0.1f && Math.Abs(m_CurrentVelocity.Z) < 0.1f);
        float activeFriction = isStandingStill ? staticFriction : dynFriction;

        if (m_IsGrounded && groundNormal.Y < 0.99f)
        {
            Vector3 slideDir = new Vector3(groundNormal.X, 0, groundNormal.Z);

            float slideForce = (1.0f - activeFriction) * 20.0f;

            if (slideForce > 0.0f)
            {
                m_CurrentVelocity.X += slideDir.X * slideForce * deltaTime;
                m_CurrentVelocity.Z += slideDir.Z * slideForce * deltaTime;
            }
        }

        float deceleration = activeFriction * 15.0f;
        if (!m_IsGrounded)
            deceleration = 2.0f;

        if (deceleration > 0.01f)
        {
            m_CurrentVelocity.X = Mathf.Lerp(m_CurrentVelocity.X, targetVelocity.X * Speed, deceleration * deltaTime);
            m_CurrentVelocity.Z = Mathf.Lerp(m_CurrentVelocity.Z, targetVelocity.Z * Speed, deceleration * deltaTime);
        }
        else
        {
            m_CurrentVelocity.X += targetVelocity.X * Speed * 1.5f * deltaTime;
            m_CurrentVelocity.Z += targetVelocity.Z * Speed * 1.5f * deltaTime;

            m_CurrentVelocity.X = Mathf.Clamp(m_CurrentVelocity.X, -25.0f, 25.0f);
            m_CurrentVelocity.Z = Mathf.Clamp(m_CurrentVelocity.Z, -25.0f, 25.0f);
        }

        m_VerticalVelocity += m_Gravity * deltaTime;
        if ((Input.IsKeyDown(KeyCode.Space) || Input.IsGamepadButtonDown(0, GamepadButton.A)) && m_IsGrounded)
        {
            m_VerticalVelocity = JumpForce;
            m_IsGrounded = false;
        }
        if (IsHovering)
            m_VerticalVelocity = 0.0f;

        if (HasComponent<CharacterControllerComponent>())
        {
            var cct = GetComponent<CharacterControllerComponent>();

            Vector3 displacement = new Vector3(m_CurrentVelocity.X, m_VerticalVelocity, m_CurrentVelocity.Z) * deltaTime;
            CollisionFlags flags = cct.Move(displacement, deltaTime);

            bool wasGrounded = m_IsGrounded;
            m_IsGrounded = (flags & CollisionFlags.Down) != 0;

            if (m_IsGrounded)
            {
                if (!wasGrounded && m_VerticalVelocity < -5.0f && restitution > 0.2f)
                {
                    m_VerticalVelocity = Math.Abs(m_VerticalVelocity) * restitution * 1.5f;
                    m_IsGrounded = false;
                }
                else if (m_VerticalVelocity < 0.0f)
                {
                    m_VerticalVelocity = -1.0f;
                }
            }
            else if ((flags & CollisionFlags.Up) != 0 && m_VerticalVelocity > 0.0f)
            {
                m_VerticalVelocity = -1.0f;
            }
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

        if (Input.IsKeyDown(KeyCode.P))
        {
            var spotLight = FindEntityByName("Projector")?.GetComponent<SpotLightComponent>();

            if (spotLight != null)
            {
                m_IsVideoPlaying = !m_IsVideoPlaying;

                if (m_IsVideoPlaying) spotLight.PlayVideo();
                else spotLight.PauseVideo();

                Thread.Sleep(150);
            }
        }

        if (Input.IsKeyDown(KeyCode.R))
        {
            var spotLight = FindEntityByName("Projector")?.GetComponent<SpotLightComponent>();
            if (spotLight != null)
            {
                spotLight.RewindVideo();
                spotLight.PlayVideo();
            }
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