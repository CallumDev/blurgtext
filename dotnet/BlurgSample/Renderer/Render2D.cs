using System;
using OpenTK.Mathematics;
using System.Runtime.InteropServices;
using BlurgText;
using OpenTK.Graphics.OpenGL;

namespace BlurgSample.Renderer
{
    public class Render2D : IDisposable
    {
        private const string VERTEX = @"
#version 120
uniform mat4 viewprojection;

attribute vec2 v_position;
attribute vec2 v_texcoord;
attribute vec4 v_color;

varying vec2 texcoord;
varying vec4 col;
void main()
{
    gl_Position = viewprojection * vec4(v_position, 0.0, 1.0);
    texcoord = vec2(v_texcoord.x, v_texcoord.y);
    col = v_color;
}
";

        private const string FRAGMENT = @"
#version 120
uniform sampler2D texture;
varying vec2 texcoord;
varying vec4 col;
void main()
{
    gl_FragColor = col * texture2D(texture, texcoord);
}

";
        private Shader shader;
        private int vao;
        private int vbo;

        [StructLayout(LayoutKind.Sequential)]
        struct Vertex
        {
            public Vector2 Position;
            public Vector2 Texture;
            public uint Color;
            public Vertex(Vector2 pos, Vector2 tex, uint color)
            {
                Position = pos;
                Texture = tex;
                Color = color;
            }
        }

        private Vertex[] vertices = new Vertex[16384];
        private int vCount = 0;
        private IntPtr currentTexture = IntPtr.Zero;
        private List<int> textures = new List<int>();
        public unsafe Render2D()
        {
            //load texture
            shader = new Shader(VERTEX, FRAGMENT);
            shader.SetI("texture", 0);
            vao = GL.GenVertexArray();
            vbo = GL.GenBuffer();
            GL.BindVertexArray(vao);
            GL.BindBuffer(BufferTarget.ArrayBuffer, vbo);
            GL.BufferData(BufferTarget.ArrayBuffer, (IntPtr)(5 * sizeof(float) * 16384), IntPtr.Zero, BufferUsageHint.StreamDraw);
            GL.EnableVertexAttribArray(0);
            GL.VertexAttribPointer(0, 2, VertexAttribPointerType.Float, false, 5 * sizeof(float), 0);
            GL.EnableVertexAttribArray(1);
            GL.VertexAttribPointer(1, 2,  VertexAttribPointerType.Float, false, 5 * sizeof(float), 2 * sizeof(float));
            GL.EnableVertexAttribArray(2);
            GL.VertexAttribPointer(2, 4, VertexAttribPointerType.UnsignedByte, true, 5 * sizeof(float), 4 * sizeof(float));
        }
        
        public IntPtr CreateTexture(int width, int height)
        {
            var texture = GL.GenTexture();
            GL.BindTexture(TextureTarget.Texture2D, texture);
            GL.TexImage2D(TextureTarget.Texture2D, 0, PixelInternalFormat.Rgba, width, height, 0,
                PixelFormat.Rgba, PixelType.UnsignedByte, IntPtr.Zero);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMinFilter, (int)TextureMinFilter.Linear);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureMagFilter, (int)TextureMagFilter.Linear);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapS, (int)TextureWrapMode.Clamp);
            GL.TexParameter(TextureTarget.Texture2D, TextureParameterName.TextureWrapT, (int)TextureWrapMode.Clamp);
            textures.Add(texture);
            return (IntPtr)texture;
        }

        public void UpdateTexture(IntPtr texture, IntPtr buffer, int x, int y, int width, int height)
        {
            GL.BindTexture(TextureTarget.Texture2D, (uint)texture);
            GL.TexSubImage2D(TextureTarget.Texture2D, 0, x, y, width, height, PixelFormat.Rgba, PixelType.UnsignedByte,
                buffer);
        }
        
        public void Start(int width, int height)
        {
            GL.Disable(EnableCap.CullFace);
            GL.Disable(EnableCap.DepthTest);
            GL.Enable(EnableCap.Blend);
            GL.BlendFunc(BlendingFactor.SrcAlpha, BlendingFactor.OneMinusSrcAlpha);
            shader.Use();
            var m = Matrix4.CreateOrthographicOffCenter(0, width, height, 0, 0, 1);
            shader.Set("viewprojection", m);
        }

        public void DrawRects(BlurgResult? rects, int x, int y)
        {
            if (rects == null)
                return;
            for (int i = 0; i < rects.Count; i++)
            {
                if (vCount + 6 > vertices.Length ||
                    (currentTexture != rects[i].UserData && currentTexture != IntPtr.Zero))
                    Flush();
                currentTexture = rects[i].UserData;
                var o = new Vector2(x, y);
                var pos = new Vector2(rects[i].X, rects[i].Y);
                var tl = new Vertex(
                    o + pos,
                    new Vector2(rects[i].U0, rects[i].V0),
                    rects[i].Color
                );
                var tr = new Vertex(
                    o + pos + new Vector2(rects[i].Width, 0),
                    new Vector2(rects[i].U1, rects[i].V0),
                    rects[i].Color
                );
                var bl = new Vertex(
                    o + pos + new Vector2(0, rects[i].Height),
                    new Vector2(rects[i].U0, rects[i].V1),
                    rects[i].Color
                );
                var br = new Vertex(
                    o + pos + new Vector2(rects[i].Width, rects[i].Height),
                    new Vector2(rects[i].U1, rects[i].V1),
                    rects[i].Color
                );
                vertices[vCount++] = tl;
                vertices[vCount++] = tr;
                vertices[vCount++] = bl;
                vertices[vCount++] = bl;
                vertices[vCount++] = tr;
                vertices[vCount++] = br;
            }
        }
        

        public void FillBackground(int x, int y, int width, int height, BlurgColor color)
        {
            if (vCount + 6 > vertices.Length)
                Flush();
            var tcoord = new Vector2(0.5f / 1024.0f, 0.5f / 1024.0f);
            var tl = new Vertex(
                new Vector2(x, y),
                tcoord,
                color
            );
            var tr = new Vertex(
                new Vector2(x + width, y),
                tcoord,
                color
            );
            var bl = new Vertex(
                new Vector2(x, y + height),
                tcoord,
                color
            );
            var br = new Vertex(
                new Vector2(x + width, y + height),
                tcoord,
                color
            );
            vertices[vCount++] = tl;
            vertices[vCount++] = tr;
            vertices[vCount++] = bl;
            vertices[vCount++] = bl;
            vertices[vCount++] = tr;
            vertices[vCount++] = br;
        }

        void Flush()
        {
            GL.BindVertexArray(vao);
            GL.BindBuffer(BufferTarget.ArrayBuffer, vbo);
            GL.BufferSubData(BufferTarget.ArrayBuffer, IntPtr.Zero, vCount * sizeof(float) * 5, vertices);
            GL.BindTexture(TextureTarget.Texture2D, (uint)currentTexture);
            GL.DrawArrays(PrimitiveType.Triangles, 0, vCount);
            vCount = 0;
        }

        public void Finish()
        {
            if (vCount > 0)
                Flush();
        }

        public void Dispose()
        {
            foreach (var t in textures)
                GL.DeleteTexture(t);
            textures.Clear();
            GL.DeleteVertexArray(vao);
            GL.DeleteBuffer(vbo);
            GL.DeleteProgram(shader.ID);
        }
        
    }
}