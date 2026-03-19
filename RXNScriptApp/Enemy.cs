using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using RXNEngine;

namespace RXNScriptApp
{
    public class Enemy : Entity
    {
        public float m_Pitch = 0.0f;

        public override void OnCreate()
        {
            Vector3 currentRot = this.Rotation;
            m_Pitch = currentRot.X;
        }

        public override void OnUpdate(float deltaTime)
        {
            m_Pitch = Math.Clamp(m_Pitch, -1.5f, 1.5f);

            this.Rotation = new Vector3(m_Pitch, 0.0f, 0.0f);
        }
    }
}
