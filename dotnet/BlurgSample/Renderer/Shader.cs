using System;
using System.Collections.Generic;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;

namespace BlurgSample.Renderer
{
    public class Shader
    {
        public int ID;
        public Shader(string vsource, string fsource)
        {
            int status;
            var vert = GL.CreateShader(ShaderType.VertexShader);
            GL.ShaderSource(vert, vsource);
            GL.CompileShader(vert);
            Console.WriteLine(GL.GetShaderInfoLog(vert));
            GL.GetShader(vert, ShaderParameter.CompileStatus, out status);
            if (status == 0) throw new Exception("Compile failed");
            var frag = GL.CreateShader(ShaderType.FragmentShader);
            GL.ShaderSource(frag, fsource);
            GL.CompileShader(frag);
            Console.WriteLine(GL.GetShaderInfoLog(frag));
            GL.GetShader(frag, ShaderParameter.CompileStatus, out status);
            if (status == 0) throw new Exception("Compile failed");
            ID = GL.CreateProgram();
            GL.AttachShader(ID, vert);
            GL.AttachShader(ID, frag);
            GL.BindAttribLocation(ID, 0, "v_position");
            GL.BindAttribLocation(ID, 1, "v_texcoord");
            GL.BindAttribLocation(ID, 2, "v_color");

            GL.LinkProgram(ID);
            Console.WriteLine(GL.GetProgramInfoLog(ID));
            GL.GetProgram(ID, ProgramParameter.LinkStatus, out status);
            if (status == 0) throw new Exception("Link failed");
        }
        
        private Dictionary<string, int> locs = new Dictionary<string, int>();


        private static int usedProgram = -1;
        
        public int GetLocation(string s)
        {
            if (!locs.TryGetValue(s, out int l))
            {
                l = GL.GetUniformLocation(ID, s);
                locs[s] = l;
            }
            return l;
        }

        public void SetI(string loc, int v)
        {
            Use();
            var x = GetLocation(loc);
            if(x != -1) GL.Uniform1(x, v);
        }
        
        public unsafe void Set(string loc, Matrix4 mat)
        {
            Use();
            var x = GetLocation(loc);
            if (x == -1) return;
            GL.UniformMatrix4(x,  false, ref mat);
        }

        public void Use()
        {
            if (usedProgram != ID)
            {
                GL.UseProgram(ID);
                usedProgram = ID;
            }
        }
    }
}