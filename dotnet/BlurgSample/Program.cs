using System.Diagnostics;
using BlurgSample.Renderer;
using BlurgText;
using OpenTK;
using OpenTK.Graphics.OpenGL;
using OpenTK.Mathematics;
using OpenTK.Windowing.Common;
using OpenTK.Windowing.Desktop;

namespace BlurgSample;

public class Program : GameWindow
{
    private Render2D renderer = null!;
    private Blurg blurg = null!;
    
    public Program(GameWindowSettings gameWindowSettings, NativeWindowSettings nativeWindowSettings) : base(gameWindowSettings, nativeWindowSettings)
    {
    }

    string Res(string p)
    {
        var folder = Path.GetDirectoryName(Process.GetCurrentProcess().MainModule!.FileName)!;
        return Path.Combine(folder, p);
    }

    private BlurgFormattedText[] texts;
    
    protected override void OnLoad()
    {
        base.OnLoad();
        renderer = new Render2D();
        blurg = new Blurg(renderer.CreateTexture, renderer.UpdateTexture);
        blurg.AddFontFile(Res("Roboto-Regular.ttf"));
        blurg.AddFontFile(Res("Roboto-Bold.ttf"));
        blurg.AddFontFile(Res("Roboto-Italic.ttf"));
        blurg.AddFontFile(Res("Roboto-BoldItalic.ttf"));
        blurg.AddFontFile(Res("Roboto-ThinItalic.ttf"));
        blurg.AddFontFile(Res("Roboto-Black.ttf"));

        texts = new BlurgFormattedText[3];
        texts[0] = new BlurgFormattedText("Here is a formatted text paragraph",
            blurg.QueryFont("Roboto", FontWeight.Bold, false)!);
        texts[0].DefaultSize = 24.0f;
        texts[1] = new BlurgFormattedText("And a centre-aligned paragraph", blurg.QueryFont("Roboto", FontWeight.Bold, true)!);
        texts[1].DefaultSize = 24.0f;
        texts[1].Alignment = BlurgAlignment.Center;
        texts[2] = new BlurgFormattedText("And text modified with a span (along with a background to test multi-paragraph measurement!)", blurg.QueryFont("Roboto", FontWeight.Regular, false)!);
        texts[2].DefaultSize = 24.0f;
        texts[2].Spans = new BlurgStyleSpan[1];
        texts[2].Spans![0] = new BlurgStyleSpan()
        {
            FontSize = 24.0f,
            Font = blurg.QueryFont("Roboto", FontWeight.Regular, false)!,
            Color = new BlurgColor(255, 0, 0, 255),
            Underline = new BlurgUnderline() { Enabled = true },
            StartIndex = 4,
            EndIndex = 10
        };
    }

    protected override void OnRenderFrame(FrameEventArgs args)
    {
        var sz = this.ClientSize;
        GL.Viewport(0, 0, sz.X, sz.Y);
        GL.ClearColor(new Color4(0.15f, 0.15f, 0.15f, 1.0f));
        GL.Clear(ClearBufferMask.ColorBufferBit | ClearBufferMask.DepthBufferBit);
        
        renderer.Start(sz.X, sz.Y);

        renderer.DrawRects(blurg.BuildString(blurg.QueryFont("Roboto", FontWeight.Bold, false)!,
            24.0f, BlurgColor.White, "Hello World from C#!"), 16, 16);

        var r = blurg.MeasureString(blurg.QueryFont("Roboto", FontWeight.Bold, false)!, 24.0f,
            "Testing measuring 1, rectangle should fit text bounds");
        renderer.FillBackground(120, 64, (int)r.X, (int)r.Y, new BlurgColor(96,96,96,255));
        renderer.DrawRects(blurg.BuildString(blurg.QueryFont("Roboto", FontWeight.Bold, false)!,
            24.0f, BlurgColor.White, "Testing measuring 1, rectangle should fit text bounds"), 120, 64);

        var sz2 = blurg.MeasureFormattedText(texts, 500);
        renderer.FillBackground(16, 190, (int)sz2.X, (int)sz2.Y, new BlurgColor(96, 0, 96, 255));
        renderer.DrawRects(blurg.BuildFormattedText(texts, 500), 16, 190);
        renderer.Finish();
        
        SwapBuffers();
        base.OnRenderFrame(args);
    }

    protected override void OnUnload()
    {
        blurg.Dispose();
        renderer.Dispose();
        base.OnUnload();
    }

    public static void Main(string[] args)
    {
        var settings = NativeWindowSettings.Default;
        settings.ClientSize = new Vector2i(1024, 768);
        settings.Vsync = VSyncMode.On;
        using var p = new Program(GameWindowSettings.Default, settings);
        p.Run();
    }
}