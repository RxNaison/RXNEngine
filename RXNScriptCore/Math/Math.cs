using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RXNEngine
{
    public static class Mathf
    {
        public static float Clamp(float value, float min, float max)
        {
            if (value < min) return min;
            if (value > max) return max;
            return value;
        }

        public static float Lerp(float a, float b, float t)
        {
            return a + (b - a) * Clamp(t, 0.0f, 1.0f);
        }
    }
}
