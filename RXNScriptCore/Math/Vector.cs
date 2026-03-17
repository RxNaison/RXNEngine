using System.Runtime.InteropServices;

namespace RXNEngine
{
    [StructLayout(LayoutKind.Sequential)]
    public struct Vector2
    {
        public float X, Y;

        public Vector2(float x, float y)
        {
            X = x;
            Y = y;
        }

        public static Vector2 operator +(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X + b.X, a.Y + b.Y);
        }

        public static Vector2 operator -(Vector2 a, Vector2 b)
        {
            return new Vector2(a.X - b.X, a.Y - b.Y);
        }

        public static Vector2 operator *(Vector2 a, float scalar)
        {
            return new Vector2(a.X * scalar, a.Y * scalar);
        }

        public static Vector2 operator /(Vector2 a, float scalar)
        {
            return new Vector2(a.X / scalar, a.Y / scalar);
        }

        public override string ToString() => $"Vector2({X}, {Y})";
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector3
    {
        public float X, Y, Z;

        public Vector3(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }

        public static Vector3 operator +(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X + b.X, a.Y + b.Y, a.Z + b.Z);
        }

        public static Vector3 operator -(Vector3 a, Vector3 b)
        {
            return new Vector3(a.X - b.X, a.Y - b.Y, a.Z - b.Z);
        }

        public static Vector3 operator *(Vector3 a, float scalar)
        {
            return new Vector3(a.X * scalar, a.Y * scalar, a.Z * scalar);
        }

        public static Vector3 operator /(Vector3 a, float scalar)
        {
            return new Vector3(a.X / scalar, a.Y / scalar, a.Z / scalar);
        }

        public override string ToString() => $"Vector3({X}, {Y}, {Z})";
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct Vector4
    {
        public float X, Y, Z, W;

        public Vector4(float x, float y, float z, float w)
        {
            X = x;
            Y = y;
            Z = z;
            W = w;
        }

        public static Vector4 operator +(Vector4 a, Vector4 b)
        {
            return new Vector4(a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W);
        }

        public static Vector4 operator -(Vector4 a, Vector4 b)
        {
            return new Vector4(a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.Z - b.Z);
        }

        public static Vector4 operator *(Vector4 a, float scalar)
        {
            return new Vector4(a.X * scalar, a.Y * scalar, a.Z * scalar, a.W * scalar);
        }

        public static Vector4 operator /(Vector4 a, float scalar)
        {
            return new Vector4(a.X / scalar, a.Y / scalar, a.Z / scalar, a.W / scalar);
        }

        public override string ToString() => $"Vector4({X}, {Y}, {Z}, {W})";
    }
}