// render.cpp - Direct3D 11 implementation of render.h.
#include "render.h"
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>

template <class T> static void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }
static int SupportedSamples(ID3D11Device* dev, int wanted);

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
    float4 fog;           // rgb = fog colour, a = density (0 = off)
    float4 shift;         // xy = whole-image offset (NDC), z = scale (pixel orbit)
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
    o.pos.xy = o.pos.xy * shift.z + shift.xy * o.pos.w;
    o.nrm = mul(worldView, float4(i.nrm, 0)).xyz;
    o.uv = i.uv;
    o.col = i.col;
    return o;
}
float3 Fog(float3 c, float3 vpos) {
    return lerp(c, fog.rgb, 1 - exp(-fog.a * length(vpos)));
}
float4 PS(PSIn i) : SV_Target {
    float4 base = flags.z > 0.5 ? i.col : matColor;
    if (flags.y > 0.5) base *= tex.Sample(samp, i.uv);
    if (flags.x < 0.5) return float4(Fog(base.rgb, i.vpos), base.a);
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
    return float4(Fog(base.rgb * diff + spec * specular.rgb + rim * base.rgb * 4, i.vpos), base.a);
}
)";

// Style post pass: full-screen triangle that reads the finished scene.
static const char kPostShader[] = R"(
cbuffer P : register(b0) {
    float4 stops[5];
    float4 grade;   // hue (radians), saturation, contrast, brightness
    float4 info;    // mode (-1 = none), strength, pattern, effect
    float4 tr;      // time, width, height, aspect
};
Texture2D src : register(t0);
SamplerState ls : register(s0);
struct V { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
V PVS(uint id : SV_VertexID) {
    V o;
    float2 p = float2((id << 1) & 2, id & 2);
    o.uv = p;
    o.pos = float4(p * float2(2, -2) + float2(-1, 1), 0, 1);
    return o;
}
float Lum(float3 c) { return dot(c, float3(0.2126, 0.7152, 0.0722)); }
// atan2 written out (some HLSL compilers, e.g. Wine's, lack it).
float Atan2(float y, float x) {
    float ax = abs(x), ay = abs(y);
    float a = min(ax, ay) / max(max(ax, ay), 1e-8);
    float s = a * a;
    float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
    if (ay > ax) r = 1.57079637 - r;
    if (x < 0) r = 3.14159274 - r;
    return y < 0 ? -r : r;
}
float Hash(float2 p) { return frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453); }
float3 Ramp(float x) {
    x = saturate(x) * 4;
    int i = min((int)x, 3);
    return lerp(stops[i].rgb, stops[i + 1].rgb, x - i);
}
float3 HueRotate(float3 c, float a) {
    const float3 k = float3(0.57735, 0.57735, 0.57735);
    float ca = cos(a);
    return c * ca + cross(k, c) * sin(a) + k * dot(k, c) * (1 - ca);
}
float3 Theme(float3 c) {
    int mode = (int)info.x;
    if (mode < 0) return c;
    float l = Lum(c);
    float3 t;
    if (mode == 0)      t = Ramp(l);
    else if (mode == 1) t = lerp(stops[0].rgb, stops[3].rgb, smoothstep(0, 1, l)) + stops[4].rgb * pow(l, 4);
    else if (mode == 2) t = c * 0.45 + l * stops[3].rgb * 1.5;
    else if (mode == 3) t = c;
    else if (mode == 4) t = lerp(Ramp(l), 1, 0.35) * (0.25 + 0.75 * sqrt(l));
    else if (mode == 5) t = Ramp(pow(l, 0.6)) * 1.35;
    else if (mode == 6) t = l * stops[3].rgb / max(max(stops[3].r, max(stops[3].g, stops[3].b)), 0.1);
    else                t = c + (stops[1].rgb - 0.5) * 0.45 * (1 - l) + (stops[3].rgb - 0.5) * 0.45 * l;
    t = HueRotate(t, grade.x);
    t = lerp(Lum(t), t, grade.y);
    t = (t - 0.5) * grade.z + 0.5;
    t = max(t, 0) * grade.w;
    // True black stays black (OLED savers rely on it).
    t *= saturate(max(c.r, max(c.g, c.b)) * 60);
    return lerp(c, t, info.y);
}
float2 Pattern(float2 uv) {
    int pat = (int)info.z;
    float t = tr.x, a = tr.w;
    float2 c = (uv - 0.5) * float2(a, 1);
    float r = length(c), ang = Atan2(c.y, c.x);
    if (pat == 1) uv.x = uv.x > 0.5 ? 1 - uv.x : uv.x;
    else if (pat == 2) uv.y = uv.y > 0.5 ? 1 - uv.y : uv.y;
    else if (pat == 3) uv = 0.5 - abs(uv - 0.5);
    else if (pat >= 4 && pat <= 7) {
        float n = pat == 4 ? 4 : pat == 5 ? 6 : pat == 6 ? 8 : 12;
        float s = 6.2831853 / n;
        float b = ang + t * 0.05;
        b = b - s * floor(b / s);
        if (b > s * 0.5) b = s - b;
        c = r * float2(cos(b), sin(b));
        uv = c / float2(a, 1) + 0.5;
    }
    else if (pat == 8) uv = frac(uv * 2);
    else if (pat == 9) uv = frac(uv * 3);
    else if (pat == 10) {
        float k = pow(1 - saturate(r / 0.75), 2) * 2.5 * sin(t * 0.3);
        float b = ang + k;
        uv = r * float2(cos(b), sin(b)) / float2(a, 1) + 0.5;
    }
    else if (pat == 11) uv += (r > 0.001 ? c / r : 0) * sin(r * 40 - t * 3) * 0.006 / float2(a, 1);
    else if (pat == 12) uv = c * lerp(0.45, 1.0, saturate(r * 1.6)) / float2(a, 1) + 0.5;
    else if (pat == 13) {
        float b = t * 0.03, cs = cos(b), sn = sin(b);
        c = float2(c.x * cs - c.y * sn, c.x * sn + c.y * cs) / sqrt(a * a + 1) * 1.02;
        uv = c / float2(a, 1) + 0.5;
    }
    else if (pat == 14) uv = c * (1 - 0.08 * (0.5 + 0.5 * sin(t * 0.6))) / float2(a, 1) + 0.5;
    else if (pat == 15) { float2 px = tr.yz / 6; uv = (floor(uv * px) + 0.5) / px; }
    else if (pat == 16) uv = float2(abs(ang) / 3.14159265, frac(0.15 / (r + 0.02) + t * 0.1));   // mirrored angle: no seam
    return uv;
}
float4 PPS(V i) : SV_Target {
    float2 uv = Pattern(i.uv);
    int fx = (int)info.w;
    float t = tr.x;
    float2 px = 1 / tr.yz;
    if (fx == 7) {   // glitch: occasional sideways-torn bands
        float band = floor(uv.y * 40), slot = floor(t * 8);
        if (Hash(float2(band, slot)) > 0.93) uv.x += (Hash(float2(slot, band)) - 0.5) * 0.08;
    }
    float3 c;
    if (fx == 4 || fx == 7) {   // chromatic aberration
        float2 d = (uv - 0.5) * (fx == 4 ? 0.008 : 0.004);
        c = float3(src.SampleLevel(ls, uv + d, 0).r, src.SampleLevel(ls, uv, 0).g, src.SampleLevel(ls, uv - d, 0).b);
    } else c = src.SampleLevel(ls, uv, 0).rgb;
    float3 o = Theme(c);
    float v = length(i.uv - 0.5);
    if (fx == 1) o *= 1 - 0.6 * pow(saturate(v * 1.3), 2);
    else if (fx == 2) o *= (0.78 + 0.22 * cos(i.pos.y * 3.14159)) * (0.9 + 0.1 * float3(fmod(i.pos.x, 3) < 1, fmod(i.pos.x + 1, 3) < 1, fmod(i.pos.x + 2, 3) < 1));
    else if (fx == 3) o += (Hash(i.pos.xy + frac(t) * 97) - 0.5) * 0.07 * saturate(Lum(o) * 20);
    else if (fx == 5) o = floor(o * 5 + 0.5) / 5;
    else if (fx == 6) o = o * 0.7 + Theme(src.SampleLevel(ls, uv, 3).rgb) * 0.45 + Theme(src.SampleLevel(ls, uv, 5).rgb) * 0.3;
    else if (fx == 8) { float l = Lum(o); o = float3(0.15, 1, 0.25) * l * 1.4; o += (Hash(i.pos.xy + frac(t) * 31) - 0.5) * 0.08 * saturate(l * 20); o *= 1 - 0.7 * pow(saturate(v * 1.4), 2); }
    else if (fx == 9) { float l = Lum(o); o = float3(1.07, 0.74, 0.43) * l * (0.95 + 0.05 * sin(t * 23)); o *= 1 - 0.5 * pow(saturate(v * 1.3), 2); }
    else if (fx == 10) { float3 b = Theme(src.SampleLevel(ls, uv, 3).rgb) + Theme(src.SampleLevel(ls, uv, 5).rgb); o += max(b * 0.5 - 0.45, 0) * 1.2; }
    else if (fx == 11) {
        float l0 = Lum(c);
        float e = abs(Lum(src.SampleLevel(ls, uv + float2(px.x, 0), 0).rgb) - l0) + abs(Lum(src.SampleLevel(ls, uv + float2(0, px.y), 0).rgb) - l0);
        o = floor(o * 4 + 0.5) / 4 * (1 - saturate(e * 6));
    }
    return float4(saturate(o), 1);
}
)";

struct PostConstants { float stops[5][4]; float grade[4]; float info[4]; float tr[4]; };

struct Constants {
    float worldView[16];
    float proj[16];
    float matColor[4];
    float specular[4];
    float lightDir[2][4];
    float lightColor[2][4];
    float ambient[4];
    float flags[4];
    float fog[4];
    float shift[4];
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
    SafeRelease(postVs); SafeRelease(postPs); SafeRelease(postCb); SafeRelease(postSampler);
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
    // Style post pass (optional: without it styles are simply not applied).
    if (compile) {
        compile(kPostShader, sizeof(kPostShader) - 1, "post", nullptr, nullptr, "PVS", "vs_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &vsb, nullptr);
        compile(kPostShader, sizeof(kPostShader) - 1, "post", nullptr, nullptr, "PPS", "ps_4_0", D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &psb, nullptr);
        if (vsb && psb) {
            dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &postVs);
            dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &postPs);
        }
        SafeRelease(vsb); SafeRelease(psb);
    }

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
    smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_MIRROR;
    dev->CreateSamplerState(&smp, &postSampler);
    bd.ByteWidth = sizeof(PostConstants);
    dev->CreateBuffer(&bd, nullptr, &postCb);

    samples = SupportedSamples(dev, wantedSamples);
    CreateTargets();
    return backRtv && dsv && persistRtv;
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
    td.SampleDesc.Count = samples;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &depthTex)))
        dev->CreateDepthStencilView(depthTex, nullptr, &dsv);
    // The scene is drawn into this multisampled (anti-aliased) target and
    // resolved into the swap chain's back buffer at Present. It also keeps
    // its contents between frames, which persistent (trail) scenes rely on.
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.BindFlags = D3D11_BIND_RENDER_TARGET;
    if (SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &persistTex))) {
        dev->CreateRenderTargetView(persistTex, nullptr, &persistRtv);
        float black[4] = { 0, 0, 0, 1 };
        if (persistRtv) ctx->ClearRenderTargetView(persistRtv, black);
    }
}

// Highest supported MSAA sample count <= wanted, for both color and depth.
static int SupportedSamples(ID3D11Device* dev, int wanted) {
    for (int n = wanted; n > 1; n /= 2) {
        UINT qc = 0, qd = 0;
        dev->CheckMultisampleQualityLevels(DXGI_FORMAT_B8G8R8A8_UNORM, n, &qc);
        dev->CheckMultisampleQualityLevels(DXGI_FORMAT_D24_UNORM_S8_UINT, n, &qd);
        if (qc > 0 && qd > 0) return n;
    }
    return 1;
}

void Renderer::ReleaseTargets() {
    if (ctx) ctx->OMSetRenderTargets(0, nullptr, nullptr);
    SafeRelease(backRtv); SafeRelease(dsv); SafeRelease(depthTex);
    SafeRelease(persistRtv); SafeRelease(persistTex);
    SafeRelease(lumSrv); SafeRelease(lumTex);
    SafeRelease(postSrv); SafeRelease(postTex);
    for (int i = 0; i < 3; i++) { SafeRelease(lumStaging[i]); lumPending[i] = false; }
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
    ID3D11RenderTargetView* rtv = persistRtv;
    ctx->OMSetRenderTargets(1, &rtv, dsv);
    D3D11_VIEWPORT vp = { 0, 0, (float)width, (float)height, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    if (clearColor) { float black[4] = { 0, 0, 0, 1 }; ctx->ClearRenderTargetView(rtv, black); }
    if (clearDepth || !persistent) ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    BindScenePipeline();
}

void Renderer::BindScenePipeline() {
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
    c.fog[0] = p.fogColor[0]; c.fog[1] = p.fogColor[1]; c.fog[2] = p.fogColor[2]; c.fog[3] = p.fogDensity;
    c.shift[0] = shiftX; c.shift[1] = shiftY; c.shift[2] = shiftScale; c.shift[3] = 0;
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

// Copies (or, with MSAA, resolves) the scene into the swap chain's buffer.
void Renderer::ResolveToBackBuffer() {
    ID3D11Texture2D* back = nullptr;
    if (SUCCEEDED(swap->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&back))) {
        if (samples > 1) ctx->ResolveSubresource(back, 0, persistTex, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
        else             ctx->CopyResource(back, persistTex);
        back->Release();
    }
}

// Scene -> screen, through the style post pass when a style is set.
void Renderer::FinishScene() {
    if (!post.Active() || !postVs || !postPs || !postCb || !postSampler) { ResolveToBackBuffer(); return; }
    if (!postTex) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = width; td.Height = height;
        td.MipLevels = 0; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
        if (FAILED(dev->CreateTexture2D(&td, nullptr, &postTex))) { ResolveToBackBuffer(); return; }
        dev->CreateShaderResourceView(postTex, nullptr, &postSrv);
        if (!postSrv) { SafeRelease(postTex); ResolveToBackBuffer(); return; }
    }
    if (samples > 1) ctx->ResolveSubresource(postTex, 0, persistTex, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    else             ctx->CopySubresourceRegion(postTex, 0, 0, 0, 0, persistTex, 0, nullptr);
    if (post.effect == 6 || post.effect == 10) ctx->GenerateMips(postSrv);   // blurred levels for glow

    PostConstants c = {};
    for (int k = 0; k < 5; k++) { c.stops[k][0] = post.stops[k][0]; c.stops[k][1] = post.stops[k][1]; c.stops[k][2] = post.stops[k][2]; }
    c.grade[0] = post.hue; c.grade[1] = post.saturation; c.grade[2] = post.contrast; c.grade[3] = post.brightness;
    c.info[0] = (float)post.mode; c.info[1] = post.strength; c.info[2] = (float)post.pattern; c.info[3] = (float)post.effect;
    c.tr[0] = post.time; c.tr[1] = (float)width; c.tr[2] = (float)height; c.tr[3] = (float)width / height;
    D3D11_MAPPED_SUBRESOURCE m;
    if (SUCCEEDED(ctx->Map(postCb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) { memcpy(m.pData, &c, sizeof(c)); ctx->Unmap(postCb, 0); }

    ctx->OMSetRenderTargets(1, &backRtv, nullptr);
    D3D11_VIEWPORT vp = { 0, 0, (float)width, (float)height, 0, 1 };
    ctx->RSSetViewports(1, &vp);
    float bf[4] = { 0, 0, 0, 0 };
    ctx->OMSetBlendState(blend[BLEND_OPAQUE], bf, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(depthOff, 0);
    ctx->IASetInputLayout(nullptr);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->VSSetShader(postVs, nullptr, 0);
    ctx->PSSetShader(postPs, nullptr, 0);
    ctx->PSSetConstantBuffers(0, 1, &postCb);
    ctx->PSSetSamplers(0, 1, &postSampler);
    ctx->PSSetShaderResources(0, 1, &postSrv);
    ctx->Draw(3, 0);
    ID3D11ShaderResourceView* none = nullptr;
    ctx->PSSetShaderResources(0, 1, &none);
    BindScenePipeline();
}

void Renderer::BeginOverlay() {
    if (overlayActive || !persistTex) return;
    FinishScene();
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
    if (!overlayActive) FinishScene();
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

// ---------------------------------------------------------------------------
// Pixel orbit and luminance snapshots (OLED burn-in guard, see restkit.h)
// ---------------------------------------------------------------------------
void Renderer::SetPixelShift(float dxPixels, float dyPixels, float scale) {
    shiftX = 2 * dxPixels / width;
    shiftY = 2 * dyPixels / height;
    shiftScale = scale;
}

void Renderer::CreateLumTargets() {
    int level = 0;
    while ((width >> level) > 64 || (height >> level) > 64) level++;
    lumLevel = level;
    lumW = width >> level; if (lumW < 1) lumW = 1;
    lumH = height >> level; if (lumH < 1) lumH = 1;
    D3D11_TEXTURE2D_DESC td = {};
    td.Width = width;
    td.Height = height;
    td.MipLevels = level + 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    if (FAILED(dev->CreateTexture2D(&td, nullptr, &lumTex))) return;
    dev->CreateShaderResourceView(lumTex, nullptr, &lumSrv);
    D3D11_TEXTURE2D_DESC sd = {};
    sd.Width = lumW;
    sd.Height = lumH;
    sd.MipLevels = 1;
    sd.ArraySize = 1;
    sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    sd.SampleDesc.Count = 1;
    sd.Usage = D3D11_USAGE_STAGING;
    sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    for (int i = 0; i < 3; i++) dev->CreateTexture2D(&sd, nullptr, &lumStaging[i]);
}

void Renderer::RequestLuminance() {
    if (!lumTex) CreateLumTargets();
    if (!lumTex || !lumSrv || !lumStaging[lumWrite] || lumPending[lumWrite]) return;
    // Average the current scene down to a tiny image on the GPU (mip chain)
    // and queue a copy for the CPU; it is read a frame or two later, so the
    // GPU never stalls.
    if (samples > 1) ctx->ResolveSubresource(lumTex, 0, persistTex, 0, DXGI_FORMAT_B8G8R8A8_UNORM);
    else             ctx->CopySubresourceRegion(lumTex, 0, 0, 0, 0, persistTex, 0, nullptr);
    ctx->GenerateMips(lumSrv);
    ctx->CopySubresourceRegion(lumStaging[lumWrite], 0, 0, 0, 0, lumTex, lumLevel, nullptr);
    lumPending[lumWrite] = true;
    lumWrite = (lumWrite + 1) % 3;
}

bool Renderer::PollLuminance(std::vector<float>& out, int& gw, int& gh) {
    for (int k = 1; k <= 3; k++) {
        int i = (lumWrite + k) % 3;   // oldest first
        if (!lumPending[i]) continue;
        D3D11_MAPPED_SUBRESOURCE m;
        HRESULT hr = ctx->Map(lumStaging[i], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING) return false;
        if (FAILED(hr)) { lumPending[i] = false; continue; }
        out.resize((size_t)lumW * lumH);
        for (int y = 0; y < lumH; y++) {
            const unsigned char* row = (const unsigned char*)m.pData + (size_t)y * m.RowPitch;
            for (int x = 0; x < lumW; x++) {
                const unsigned char* p = row + x * 4;   // B, G, R, A
                out[(size_t)y * lumW + x] = (0.0722f * p[0] + 0.7152f * p[1] + 0.2126f * p[2]) / 255.0f;
            }
        }
        ctx->Unmap(lumStaging[i], 0);
        lumPending[i] = false;
        gw = lumW; gh = lumH;
        return true;
    }
    return false;
}
