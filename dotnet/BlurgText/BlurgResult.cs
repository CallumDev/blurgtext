using System;
using System.Collections;
using System.Collections.Generic;

namespace BlurgText
{
    public sealed unsafe class BlurgResult : 
        IDisposable, IReadOnlyList<BlurgRect>
    {
        public float Width => res.width;
        public float Height => res.height;
        public int Count => res.rectCount;
        
        private BlurgNative.blurg_result_t res;

        public ReadOnlySpan<BlurgCursor> Cursors => res.cursors == IntPtr.Zero
            ? ReadOnlySpan<BlurgCursor>.Empty
            : new ReadOnlySpan<BlurgCursor>((void*)res.cursors, res.cursorCount);

        internal BlurgResult(BlurgNative.blurg_result_t res)
        {
            this.res = res;
        }

        private void ReleaseUnmanagedResources()
        {
            fixed (BlurgNative.blurg_result_t* r = &res)
            {
                BlurgNative.blurg_free_result((IntPtr)r);
            }
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
        
        public BlurgRect this[int index] => ((BlurgRect*)res.rects)[index];

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