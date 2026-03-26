using System.Collections;
using System.Collections.Generic;

namespace RXNEngine
{
    public abstract class YieldInstruction { }

    public class WaitForSeconds : YieldInstruction
    {
        public float TimeLeft;
        public WaitForSeconds(float time) { TimeLeft = time; }
    }

    public class CoroutineRunner
    {
        private List<IEnumerator> _activeCoroutines = new List<IEnumerator>();

        public void StartCoroutine(IEnumerator routine)
        {
            _activeCoroutines.Add(routine);
        }

        public void Update()
        {
            for (int i = _activeCoroutines.Count - 1; i >= 0; i--)
            {
                IEnumerator routine = _activeCoroutines[i];

                if (routine.Current is WaitForSeconds waitTimer)
                {
                    waitTimer.TimeLeft -= Time.DeltaTime;

                    if (waitTimer.TimeLeft > 0.0f)
                        continue;
                }

                bool isFinished = !routine.MoveNext();

                if (isFinished)
                {
                    _activeCoroutines.RemoveAt(i);
                }
            }
        }
    }
}