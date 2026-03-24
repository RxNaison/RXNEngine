using RXNEngine;

public class Cube : Entity
{
    public override void OnFixedUpdate(float deltaTime)
    {
        float result = 0.0f;
        for (int i = 0; i < 50000; i++)
        {
            result += (float)Math.Sqrt(i) * (float)Math.Sin(i);
        }

        Vector3 pos = this.Translation;
        pos.Y += result * 0.0000001f;
        this.Translation = pos;
    }
}
