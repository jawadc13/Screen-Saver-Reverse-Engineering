// render.h - small Direct3D 11 renderer, one instance per monitor.
//
// Why Direct3D 11 and not OpenGL: on GeForce cards an OpenGL context always
// renders on one GPU and its vsync follows the primary monitor. On a PC with
// a different GPU (and refresh rate) per monitor, the other monitor's frames
// get copied between GPUs and paced to the wrong refresh rate - choppy.
// Like the original D3DSaver (one Direct3D device per adapter), each
// Renderer creates its device on the GPU that drives its monitor and
// presents with a flip-model swap chain synced to that monitor.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#include <windows.h>
#include "mesh.h"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Buffer;
struct ID3D11Texture2D;
struct ID3D11ShaderResourceView;
struct ID3D11RenderTargetView;
struct ID3D11DepthStencilView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11InputLayout;
struct ID3D11BlendState;
struct ID3D11DepthStencilState;
struct ID3D11RasterizerState;
struct ID3D11SamplerState;
struct IDXGISwapChain1;

enum BlendMode { BLEND_OPAQUE, BLEND_ALPHA, BLEND_ADD, BLEND_SUBTRACT };   // subtract: dest - src

struct Texture {
    ID3D11ShaderResourceView* srv = nullptr;
    ~Texture();
};

// Vertex buffer that lives on the GPU and can grow by appending (Pipes).
struct GpuMesh {
    ID3D11Buffer* buffer = nullptr;
    size_t capacity = 0, uploaded = 0;
    ~GpuMesh();
    void Reset() { uploaded = 0; }
};

struct DrawParams {
    Mat4      world = Mat4::Identity();
    bool      lit = true;
    bool      vertexColor = true;          // else use `color`
    bool      depth = true;
    BlendMode blend = BLEND_OPAQUE;
    float     color[4] = { 1, 1, 1, 1 };
    float     specular[3] = { 0, 0, 0 };
    float     shininess = 32;
    Texture*  texture = nullptr;
    float     rim = 0;                     // edge glow strength (lit only)
};

class Renderer {
public:
    ~Renderer();
    // Anti-aliasing: MSAA sample count to use (1 = off); call before Create.
    // The highest supported count up to this is chosen.
    void SetMultisample(int count) { wantedSamples = count < 1 ? 1 : count; }
    int  Multisample() const { return samples; }
    bool Create(HWND hwnd, HMONITOR monitor, int width, int height);
    void Resize(int width, int height);
    int  Width() const { return width; }
    int  Height() const { return height; }
    // Seconds per refresh of this renderer's monitor (exact, e.g. 1/59.94).
    double RefreshPeriod() const { return refreshPeriod; }

    // Keep the previous frame's image instead of starting from undefined
    // contents (needed for motion trails with flip-model swap chains).
    void SetPersistent(bool on) { persistent = on; }

    // Blocks until this monitor is ready for a new frame (frame-latency
    // waitable object), keeping input-to-display latency at one frame.
    void WaitForFrame();
    // True when vsync pacing comes from the frame-latency waitable object.
    bool HasFrameWait() const { return frameWait != nullptr; }
    // With persistent on, clearDepth=false keeps the depth buffer as well, so
    // new geometry can be drawn into the previous frame's 3D scene.
    void BeginFrame(bool clearColor, bool clearDepth = true);
    void SetCamera(const Mat4& view, const Mat4& proj) { this->view = view; this->proj = proj; }
    // Directional lights, directions given in view (camera) space.
    void SetLight(int i, const Vec3& dir, float r, float g, float b);
    void SetAmbient(float r, float g, float b) { ambient[0] = r; ambient[1] = g; ambient[2] = b; }

    void Draw(const Vertex* v, size_t count, const DrawParams& p);      // streamed
    void Upload(GpuMesh& gm, const Mesh& m);                             // append new vertices
    void Draw(GpuMesh& gm, const DrawParams& p);
    void Draw(GpuMesh& gm, size_t first, size_t count, const DrawParams& p);
    // Switch to drawing straight onto this frame's final image (on top of a
    // persistent scene) - for overlays that must not accumulate.
    void BeginOverlay();
    void FullscreenQuad(float r, float g, float b, float a, BlendMode mode = BLEND_ALPHA);
    // Darken the kept image for trails: multiply by (1 - alpha), then also
    // subtract one 8-bit step so faint trails reach true black instead of
    // getting stuck as grey ghosts (8-bit rounding).
    void FadeToBlack(float alpha);
    void Present();   // waits for this monitor's vertical blank

    Texture* CreateTexture(const unsigned* bgra, int w, int h);

private:
    void ApplyState(const DrawParams& p);
    void ResolveToBackBuffer();
    void CreateTargets();
    void ReleaseTargets();

    HWND hwnd = nullptr;
    int  width = 0, height = 0;
    double refreshPeriod = 1.0 / 60;
    bool persistent = false;
    int  wantedSamples = 4, samples = 1;
    bool overlayActive = false;
    unsigned swapFlags = 0;
    HANDLE frameWait = nullptr;

    ID3D11Device* dev = nullptr;
    ID3D11DeviceContext* ctx = nullptr;
    IDXGISwapChain1* swap = nullptr;
    ID3D11RenderTargetView* backRtv = nullptr;
    ID3D11Texture2D* persistTex = nullptr;
    ID3D11RenderTargetView* persistRtv = nullptr;
    ID3D11Texture2D* depthTex = nullptr;
    ID3D11DepthStencilView* dsv = nullptr;
    ID3D11VertexShader* vs = nullptr;
    ID3D11PixelShader* ps = nullptr;
    ID3D11InputLayout* layout = nullptr;
    ID3D11Buffer* cb = nullptr;
    ID3D11Buffer* stream = nullptr;
    size_t streamCap = 0, streamPos = 0;
    ID3D11BlendState* blend[4] = {};
    ID3D11DepthStencilState* depthOn = nullptr;
    ID3D11DepthStencilState* depthOff = nullptr;
    ID3D11RasterizerState* raster = nullptr;
    ID3D11SamplerState* sampler = nullptr;

    Mat4  view = Mat4::Identity(), proj = Mat4::Identity();
    float lightDir[2][4] = {};
    float lightColor[2][4] = {};
    float ambient[3] = { 0.1f, 0.1f, 0.1f };
};
