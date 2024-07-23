using System;
using System.Buffers;
using System.Text;

namespace BlurgText
{
    internal ref struct UTF8ZHelper
    {
        private byte[]? poolArray;
        private Span<byte> bytes;
        private Span<byte> utf8z;
        public UTF8ZHelper(Span<byte> initialBuffer, ReadOnlySpan<char> value)
        {
            poolArray = null;
            bytes = initialBuffer;
            int maxSize = Encoding.UTF8.GetMaxByteCount(value.Length) + 1;
            if (bytes.Length < maxSize) {
                poolArray = ArrayPool<byte>.Shared.Rent(maxSize);
                bytes = new Span<byte>(poolArray);
            }
            int byteCount = Encoding.UTF8.GetBytes(value, bytes);
            bytes[byteCount] = 0;
            utf8z = bytes.Slice(0, byteCount + 1);
        }

        public Span<byte> ToUTF8Z()
        {
            return utf8z;
        }

        public void Dispose()
        {
            if (poolArray != null)
            {
                ArrayPool<byte>.Shared.Return(poolArray);
                poolArray = null;
            }
        }
    }
}