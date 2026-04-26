using RXNEngine;
using System;

public class PulsingLight : Entity
{
    public float MinIntensity = 2.0f;
    public float MaxIntensity = 50.0f;
    public float PulseSpeed = 3.0f;

    public override void OnUpdate(float deltaTime)
    {
        float sineValue = ((float)Math.Sin(Time.TimeSinceStartup * PulseSpeed) + 1.0f) * 0.5f;

        float currentIntensity = MinIntensity + (MaxIntensity - MinIntensity) * sineValue;

        if (HasComponent<PointLightComponent>())
        {
            var light = GetComponent<PointLightComponent>();
            var data = light.Data;

            data.Intensity = currentIntensity;

            light.Data = data;
        }
        else if (HasComponent<SpotLightComponent>())
        {
            var light = GetComponent<SpotLightComponent>();
            var data = light.Data;

            data.Intensity = currentIntensity;

            light.Data = data;
        }
    }
}