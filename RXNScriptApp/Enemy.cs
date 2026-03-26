using RXNEngine;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace RXNScriptApp
{
    public class Enemy : Entity
    {
        public float m_Pitch = 0.0f;

        private IEnumerator AttackPattern()
        {
            while (true)
            {
                Console.WriteLine("Enemy aims...");
                yield return new WaitForSeconds(1.0f);

                Console.WriteLine("Enemy FIRES!");

                Console.WriteLine("Enemy reloads...");
                yield return new WaitForSeconds(2.5f);
            }
        }

        public override void OnCreate()
        {
            Vector3 currentRot = this.Rotation;
            m_Pitch = currentRot.X;

            StartCoroutine(AttackPattern());
        }

        public override void OnUpdate(float deltaTime)
        {
            m_Pitch = Math.Clamp(m_Pitch, -1.5f, 1.5f);

            this.Rotation = new Vector3(m_Pitch, 0.0f, 0.0f);
        }
    }
}
