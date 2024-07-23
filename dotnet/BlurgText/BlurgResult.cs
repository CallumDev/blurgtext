using System;
using System.Collections;
using System.Collections.Generic;

namespace BlurgText
{
    public sealed unsafe class BlurgResult : IDisposable, IReadOnlyList<BlurgRect>
    {
        public float Width { get; }
        public float Height { get; }
        public int Count { get; }

        private IntPtr rects;

        internal BlurgResult(IntPtr rects, int count, float w, float h)
        {
            this.rects = rects;
            this.Count = count;
            Width = w;
            Height = h;
        }

        private void ReleaseUnmanagedResources()
        {
            BlurgNative.blurg_free_rects(rects);
        }

        public void Dispose()
        {
            ReleaseUnmanagedResources();
            GC.SuppressFinalize(this);
        }

        public struct RectEnumerator : IEnumerator<BlurgRect>
        {
            internal BlurgResult Result;
            private int index;
            
            public bool MoveNext()
            {
                if (index >= Result.Count) return false;
                Current = Result[index];
                return true;
            }

            public void Reset()
            {
                Current = default;
                index = 0;
            }

            public BlurgRect Current { get; set; }

            object? IEnumerator.Current => Current;

            public void Dispose()
            {
            }
        }

        public RectEnumerator GetEnumerator() => new RectEnumerator() { Result = this };

        IEnumerator<BlurgRect> IEnumerable<BlurgRect>.GetEnumerator() => GetEnumerator();
        
        public BlurgRect this[int index] => ((BlurgRect*)rects)[index];

        ~BlurgResult()
        {
            ReleaseUnmanagedResources();
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return GetEnumerator();
        }
    }
}