// render.cpp - Direct3D 11 implementation of render.h.
#include "render.h"
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>

template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

// HLSL. Lighting is computed per pixel in view space: two directional
// lights + ambient, Blinn-Phong specular (the look of the fixed-function
// lighting the original savers used).
static const char kShader[] = R"(
cbuffer CB : register(b0) {
    row_major float4x4 worldView;
    row_major float4x4 proj;
    float4 matColor;
    float4 specular;      // rgb, a = shininess
    float4 lightDir[2];
    float4 lightColor[2];
    float4 ambient;
    float4 flags;         // x = lit, y = textured, z = vertex color, w = rim light
};
Texture2D tex : register(t0);
SamplerState samp : register(s0);
struct VSIn { float3 pos : POSITION; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float4 col : COLOR0; };
struct PSIn { float4 pos : SV_Position; float3 vpos : TEXCOORD1; float3 nrm : NORMAL; float2 uv : TEXCOORD0; float4 col : COLOR0; };
PSIn VS(VSIn i) {
    PSIn o;
    float4 p = mul(worldView, float4(i.pos, 1));
    o.vpos = p.xyz;
    o.pos = mul(proj, p);
    o.nrm = mul(worldView, float4(i.nrm, 0)).xyz;
    o.uv = i.uv;
    o.col = i.col;
    return o;
}
float4 PS(PSIn i) : SV_Target {
    float4 base = flags.z > 0.5 ? i.col : matColor;
    if (flags.y > 0.5) base *= tex.Sample(samp, i.uv);
    if (flags.x < 0.5) return base;
    float3 n = normalize(i.nrm);
    float3 v = normalize(-i.vpos);
    float3 diff = ambient.rgb;
    float3 spec = 0;
    for (int k = 0; k < 2; k++) {
        float3 L = normalize(lightDir[k].xyz);
        float d = dot(n, L);
        diff += lightColor[k].rgb * max(d, 0);
        if (d > 0) spec += lightColor[k].rgb * pow(max(dot(n, normalize(L + v)), 0), specular.a);
    }
    // Rim light: edges seen at a grazing angle glow (glass, soap film).
    float rim = flags.w * pow(1 - saturate(abs(dot(n, v))), 3);
    return float4(base.rgb * diff + spec * specular.rgb + rim * base.rgb * 4, base.a);
}
)";

struct Constants {
    float worldView[16];
    float proj[16];
    float matColor[4];
    float specular[4];
    float lightDir[2][4];
    float lightColor[2][4];
    float ambient[4];
    float flags[4];
};

Texture::~Texture() { SafeRelease(srv); }
GpuMesh::~GpuMesh() { SafeRelease(buffer); }

// Exact refresh rate (e.g. 59.94 Hz) of the monitor; integer fallback.
static double MonitorRefreshPeriod(HMONITOR mon) {
    MONITORINFOEXW mi = {}; mi.cbSize = sizeof(mi);
    if (!GetMonitorInfoW(mon, &mi)) return 1.0 / 60;
    UINT32 np = 0, nm = 0;
    if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &np, &nm) == ERROR_SUCCESS && np) {
        std::vector<DISPLAYCONFIG_PATH_INFO> paths(np);
        std::vector<DISPLAYCONFIG_MODE_INFO> modes(nm);
        if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &np, paths.data(), &nm, modes.data(), nullptr) == ERROR_SUCCESS) {
            for (UINT32 i = 0; i < np; i++) {
                DISPLAYCONFIG_SOURCE_DEVICE_NAME src = {};
                src.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                src.header.size = sizeof(src);
                src.header.adapterId = paths[i].sourceInfo.adapterId;
                src.header.id = paths[i].sourceInfo.id;
                if (DisplayConfigGetDeviceInfo(&src.header) != ERROR_SUCCESS) continue;
                if (lstrcmpiW(src.viewGdiDeviceName, mi.szDevice) != 0) continue;
                const DISPLAYCONFIG_RATIONAL& r = paths[i].targetInfo.refreshRate;
                if (r.Numerator && r.Denominator) return (double)r.Denominator / r.Numerator;
            }
        }
    }
    DEVMODEW dm = {}; dm.dmSize = sizeof(dm);
    if (EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm) && dm.dmDisplayFrequency > 1)
        return 1.0 / dm.dmDisplayFrequency;
    return 1.0 / 60;
}

// The DXGI adapter (GPU) that has `mon` connected to one of its outputs.
static IDXGIAdapter1* AdapterForMonitor(IDXGIFactory1* f, HMONITOR mon) {
    IDXGIAdapter1* a = nullptr;
    for (UINT i = 0; f->EnumAdapters1(i, &a) != DXGI_ERROR_NOT_FOUND; i++) {
        IDXGIOutput* o = nullptr;
        for (UINT j = 0; a->EnumOutputs(j, &o) != DXGI_ERROR_NOT_FOUND; j++) {
            DXGI_OUTPUT_DESC d;
            bool match = SUCCEEDED(o->GetDesc(&d)) && d.Monitor == mon;
            o->Release();
            if (match) return a;
        }
        a->Release();
    }
    return nullptr;
}

Renderer::~Renderer() {
    if (ctx) ctx->ClearState();
    ReleaseTargets();
    for (auto*& b : blend) SafeRelease(b);
    SafeRelease(depthOn); SafeRelease(depthOff); SafeRelease(raster); SafeRelease(sampler);
    SafeRelease(stream); SafeRelease(cb); SafeRelease(layout); SafeRelease(vs); SafeRelease(ps);
    if (frameWait) CloseHandle(frameWait);
    SafeRelease(swap); SafeRelease(ctx); SafeRelease(dev);
}

bool Renderer::Create(HWND hwnd_, HMONITOR monitor, int w, int h) {
    hwnd = hwnd_;
    width = w > 0 ? w : 1;
    height = h > 0 ? h : 1;
    refreshPeriod = MonitorRefreshPeriod(monitor);

    // 1. Device on the GPU that drives this monitor.
    IDXGIFactory1* factory = nullptr;
    if (FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&factory))) return false;
    IDXGIAdapter1* adapter = AdapterForMonitor(factory, monitor);
    factory->Release();
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
    HRESULT hr = D3D11CreateDevice(adapter, adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, levels, 3, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
    if (adapter) adapter->Release();
    if (FAILED(hr))   // no usable GPU: software rasterizer
        hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, D3D11_CREATE_DEVICE_BGRA_SUPPORT,
                               levels, 3, D3D11_SDK_VERSION, &dev, nullptr, &ctx);
    if (FAILED(hr)) return false;

    // 2. Flip-model swap chain from the device's own factory.
    IDXGIDevice* dxgiDev = nullptr; IDXGIAdapter* devAdapter = nullptr; IDXGIFactory2* f2 = nullptr;
    dev->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDev);
    if (dxgiDev) dxgiDev->GetAdapter(&devAdapter);
    if (devAdapter) devAdapter->GetParent(__uuidof(IDXGIFactory2), (void**)&f2);
    SafeRelease(devAdapter); SafeRelease(dxgiDev);
    if (!f2) return false;

    DXGI_SWAP_CHAIN_DESC1 sd = {};
    sd.Width = width;
    sd.Height = height;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.Scaling = DXGI_SCALING_STRETCH;
    sd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    // FLIP_DISCARD (Windows 10+), then FLIP_SEQUENTIAL (8+), then legacy blt.
    const DXGI_SWAP_EFFECT effects[] = { (DXGI_SWAP_EFFECT)4, (DXGI_SWAP_EFFECT)3, DXGI_SWAP_EFFECT_DISCARD };
    for (DXGI_SWAP_EFFECT e : effects) {
        sd.SwapEffect = e;
        bool flip = e != DXGI_SWAP_EFFECT_DISCARD;
        sd.BufferCount = flip ? 3 : 1;   // triple buffering: slack for a slow composition
        sd.Flags = swapFlags = flip ? 0x40 /* FRAME_LATENCY_WAITABLE_OBJECT */ : 0;
        if (SUCCEEDED(f2->CreateSwapChainForHwnd(dev, hwnd, &sd, nullptr, nullptr, &swap))) break;
    }
    if (swap) f2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER | DXGI_MWA_NO_WINDOW_CHANGES);
    f2->Release();
    if (!swap) return false;

    if (swapFlags) {
        IDXGISwapChain2* s2 = nullptr;
        if (SUCCEEDED(swap->QueryInterface(__uuidof(IDXGISwapChain2), (void**)&s2))) {
            // Two frames may be queued. A rotated (portrait) monitor is
            // composed by Windows every frame; with one frame of slack, any
            // delay there drops a frame. Two absorbs it (latency doesn't
            // matter for a screensaver).
            s2->SetMaximumFrameLatency(2);
            frameWait = s2->GetFrameLatencyWaitableObject();
            s2->Release();
        }
    }

    // 3. Shaders (compiled at startup; d3dcompiler_47.dll ships with Windows 8.1+).
    HMODULE dc = LoadLibraryW(L"d3dcompiler_47.dll");
    if (!dc) dc = LoadLibraryW(L"d3dcompiler_43.dll");
    if (!dc) return false;
    auto compile = (pD3DCompile)GetProcAddress(dc, "D3DCompile");
    ID3DBlob *vsb = nullptr, *psb = nullptr;
    if (compile) {
        compile(kShader, sizeof(kShader) - 1, "saver", nullptr, nullptr, "VS", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsb, nullptr);
        compile(kShader, sizeof(kShader) - 1, "saver", nullptr, nullptr, "PS", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psb, nullptr);
    }
    bool ok = vsb && psb &&
        SUCCEEDED(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs)) &&
        SUCCEEDED(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps));
    if (ok) {
        D3D11_INPUT_ELEMENT_DESC il[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR",    0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        ok = SUCCEEDED(dev->CreateInputLayout(il, 4, vsb->GetBufferPointer(), vsb->GetBufferSize(), &layout));
    }
    SafeRelease(vsb); SafeRelease(psb);
    if (!ok) return false;

    // 4. Fixed state objects.
    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = sizeof(Constants);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    if (FAILED(dev->CreateBuffer(&bd, nullptr, &cb))) return false;

    for (int i = 0; i < 4; i++) {
        D3D11_BLEND_DESC b = {};
        auto& rt = b.RenderTarget[0];
        rt.BlendEnable = i != BLEND_OPAQUE;
        rt.SrcBlend = i == BLEND_SUBTRACT ? D3D11_BLEND_ONE : D3D11_BLEND_SRC_ALPHA;
        rt.DestBlend = (i == BLEND_ADD || i == BLEND_SUBTRACT) ? D3D11_BLEND_ONE : D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = i == BLEND_SUBTRACT ? D3D11_BLEND_OP_REV_SUBTRACT : D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_ZERO;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        dev->CreateBlendState(&b, &blend[i]);
    }
    D3D11_DEPTH_STENCIL_DESC dd = {};
    dd.DepthEnable = TRUE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    dev->CreateDepthStencilState(&dd, &depthOn);
    dd.DepthEnable = FALSE;
    dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dev->CreateDepthStencilState(&dd, &depthOff);
    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;
    dev->CreateRasterizerState(&rd, &raster);
    D3D11_SAMPLER_DESC smp = {};
    smp.Filter = D3D11_FILTER_ANISOTROPIC;
    smp.MaxAnisotropy = 8;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    smp.MaxLOD = D3D11_FLOAT32_MAX;
    dev->CreateSamplerState(&smp, &sampler);

    CreateTargets();
    return backRtv && dsv;
}

void Renderer::CreateTargets() {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) {
        dev->CreateRenderTargetView(back, nullptr, &backRtv);
        back->Release();
    }
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width;
    td.Height = height;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &depthTex)))
        dev->CreateDepthStencilView(depthTex, nullptr, &dsv);
    // Offscreen copy of the frame for persistent (trails) rendering.
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &persistTex))) {
        dev->CreateRenderTargetView(persistTex, nullptr, &persistRtv);
        float black[4] = { 0, 0, 0, 1 };
        if (persistRtv) ctx->ClearRenderTargetView(persistRtv, black);
    }
}

void Renderer::ReleaseTargets() {
    if (ctx) ctx->OMSetRenderTargets(0, nullptr, nullptr);
    SafeRelease(backRtv); SafeRelease(dsv); SafeRelease(depthTex);
    SafeRelease(persistRtv); SafeRelease(persistTex);
}

void Renderer::Resize(int w, int h) {
    if (w <= 0 || h <= 0 || (w == width && h == height)) return;
    width = w; height = h;
    ReleaseTargets();
    swap->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, swapFlags);
    CreateTargets();
}

void Renderer::WaitForFrame() {
    if (frameWait) WaitForSingleObjectEx(frameWait, 1000, TRUE);
}

void Renderer::BeginFrame(bool clearColor, bool clearDepth) {
    overlayActive = false;
    ID3D11RenderTargetView* rtv = persistent ? persistRtv : backRtv;
    ctx->OMSetRenderTargets(1, &rtv, dsv);
    D3D11_VIEWPORT vp = { 0, 0, (float)width, (float)height, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    if (clearColor) { float black[4] = { 0, 0, 0, 1 }; ctx->ClearRenderTargetView(rtv, black); }
    if (clearDepth || !persistent) ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    ctx->IASetInputLayout(layout);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(vs, nullptr, 0);
    ctx->PSSetShader(ps, nullptr, 0);
    ctx->VSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetConstantBuffers(0, 1, &cb);
    ctx->PSSetSamplers(0, 1, &sampler);
    ctx->RSSetState(raster);
}

void Renderer::SetLight(int i, const Vec3& dir, float r, float g, float b) {
    lightDir[i][0] = dir.x; lightDir[i][1] = dir.y; lightDir[i][2] = dir.z;
    lightColor[i][0] = r; lightColor[i][1] = g; lightColor[i][2] = b;
}

void Renderer::ApplyState(const DrawParams& p) {
    Constants c;
    Mat4 wv = view * p.world;
    memcpy(c.worldView, wv.m, sizeof(c.worldView));
    memcpy(c.proj, proj.m, sizeof(c.proj));
    memcpy(c.matColor, p.color, sizeof(c.matColor));
    c.specular[0] = p.specular[0]; c.specular[1] = p.specular[1]; c.specular[2] = p.specular[2];
    c.specular[3] = p.shininess;
    memcpy(c.lightDir, lightDir, sizeof(c.lightDir));
    memcpy(c.lightColor, lightColor, sizeof(c.lightColor));
    c.ambient[0] = ambient[0]; c.ambient[1] = ambient[1]; c.ambient[2] = ambient[2]; c.ambient[3] = 0;
    c.flags[0] = p.lit ? 1.0f : 0.0f;
    c.flags[1] = p.texture ? 1.0f : 0.0f;
    c.flags[2] = p.vertexColor ? 1.0f : 0.0f;
    c.flags[3] = p.rim;
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->Map(cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        memcpy(m.pData, &c, sizeof(c));
        ctx->Unmap(cb, 0);
    }
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blend[p.blend], bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(p.depth ? depthOn : depthOff, 0);
    ID3D11ShaderResourceView* srv = p.texture ? p.texture->srv : nullptr;
    ctx->PSSetShaderResources(0, 1, &srv);
}

void Renderer::Draw(const Vertex* v, size_t count, const DrawParams& p) {
    if (!count) return;
    // Streaming vertex buffer: append with NO_OVERWRITE, wrap with DISCARD.
    if (count > streamCap) {
        SafeRelease(stream);
        streamCap = count * 2 > 65536 ? count * 2 : 65536;
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(streamCap * sizeof(Vertex));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, &stream))) { streamCap = 0; return; }
        streamPos = streamCap;   // force DISCARD
    }
    D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
    if (streamPos + count > streamCap) { streamPos = 0; mode = D3D11_MAP_WRITE_DISCARD; }
    D3D11_MAPPED_SUBRESOURCE m;
    if (FAILED(ctx->Map(stream, 0, mode, 0, &m))) return;
    memcpy((Vertex*)m.pData + streamPos, v, count * sizeof(Vertex));
    ctx->Unmap(stream, 0);
    ApplyState(p);
    UINT stride = sizeof(Vertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &stream, &stride, &offset);
    ctx->Draw((UINT)count, (UINT)streamPos);
    streamPos += count;
}

void Renderer::Upload(GpuMesh& gm, const Mesh& m) {
    size_t n = m.verts.size();
    if (n <= gm.uploaded) { gm.uploaded = n; return; }
    if (n > gm.capacity) {
        SafeRelease(gm.buffer);
        gm.capacity = n + n / 2 > 65536 ? n + n / 2 : 65536;
        D3D11_BUFFER_DESC bd = {};
        bd.ByteWidth = (UINT)(gm.capacity * sizeof(Vertex));
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        if (FAILED(dev->CreateBuffer(&bd, nullptr, &gm.buffer))) { gm.capacity = 0; gm.uploaded = 0; return; }
        gm.uploaded = 0;
    }
    D3D11_BOX box = { (UINT)(gm.uploaded * sizeof(Vertex)), 0, 0, (UINT)(n * sizeof(Vertex)), 1, 1 };
    ctx->UpdateSubresource(gm.buffer, 0, &box, &m.verts[gm.uploaded], 0, 0);
    gm.uploaded = n;
}

void Renderer::Draw(GpuMesh& gm, const DrawParams& p) { Draw(gm, 0, gm.uploaded, p); }

void Renderer::Draw(GpuMesh& gm, size_t first, size_t count, const DrawParams& p) {
    if (!gm.buffer || first >= gm.uploaded) return;
    if (first + count > gm.uploaded) count = gm.uploaded - first;
    if (!count) return;
    ApplyState(p);
    UINT stride = sizeof(Vertex), offset = 0;
    ctx->IASetVertexBuffers(0, 1, &gm.buffer, &stride, &offset);
    ctx->Draw((UINT)count, (UINT)first);
}

void Renderer::BeginOverlay() {
    if (!persistent || overlayActive || !persistTex) return;
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) {
        ctx->CopyResource(back, persistTex);
        back->Release();
    }
    ctx->OMSetRenderTargets(1, &backRtv, nullptr);
    overlayActive = true;
}

void Renderer::FadeToBlack(float alpha) {
    FullscreenQuad(0, 0, 0, alpha);
    FullscreenQuad(1.0f / 255, 1.0f / 255, 1.0f / 255, 1, BLEND_SUBTRACT);
}

void Renderer::FullscreenQuad(float r, float g, float b, float a, BlendMode mode) {
    auto byte = [](float x) { return (unsigned char)(x <= 0 ? 0 : x >= 1 ? 255 : x * 255 + 0.5f); };
    unsigned char c[4] = { byte(r), byte(g), byte(b), byte(a) };
    Vertex v[6];
    float xy[6][2] = { {-1,-1}, {1,-1}, {1,1}, {-1,-1}, {1,1}, {-1,1} };
    for (int i = 0; i < 6; i++) {
        Vertex x = { xy[i][0], xy[i][1], 0.5f, 0, 0, 1, 0, 0, c[0], c[1], c[2], c[3] };
        v[i] = x;
    }
    Mat4 savedView = view, savedProj = proj;
    view = proj = Mat4::Identity();
    DrawParams p;
    p.lit = false;
    p.depth = false;
    p.blend = mode;
    Draw(v, 6, p);
    view = savedView; proj = savedProj;
}

void Renderer::Present() {
    if (persistent && persistTex && !overlayActive) {
        ID3D11Texture2D* back = nullptr;
        if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) {
            ctx->CopyResource(back, persistTex);
            back->Release();
        }
    }
    // Sync interval 1: wait for the vertical blank of *this* monitor.
    HRESULT hr = swap->Present(1, 0);
    if (hr == DXGI_STATUS_OCCLUDED) Sleep(50);   // e.g. screen locked / display off
}

Texture* Renderer::CreateTexture(const unsigned* bgra, int w, int h) {
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w;
    td.Height = h;
    td.MipLevels = 0;   // full chain
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    ID3D11Texture2D* t = nullptr;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &t))) return nullptr;
    ctx->UpdateSubresource(t, 0, nullptr, bgra, w * 4, 0);
    auto* tex = new Texture;
    dev->CreateShaderResourceView(t, nullptr, &tex->srv);
    t->Release();
    if (!tex->srv) { delete tex; return nullptr; }
    ctx->GenerateMips(tex->srv);
    return tex;
}
