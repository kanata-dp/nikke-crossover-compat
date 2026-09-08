/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <cstdio>
#include <cstring>

static int probe(ID3D11Device *producer, ID3D11Device *consumer,
                 DXGI_FORMAT format, const char *name) {
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = desc.Height = 1600;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
    ID3D11Texture2D *texture = nullptr;
    std::printf("%s: creating shared texture\n", name);
    HRESULT hr = producer->CreateTexture2D(&desc, nullptr, &texture);
    std::printf("%s CreateTexture2D=0x%08lx", name, (unsigned long)hr);
    if (FAILED(hr)) { std::puts(""); return 1; }
    IDXGIResource *resource = nullptr;
    hr = texture->QueryInterface(__uuidof(IDXGIResource), (void **)&resource);
    if (FAILED(hr)) { texture->Release(); std::puts(" QueryInterface failed"); return 1; }
    HANDLE shared = nullptr;
    hr = resource->GetSharedHandle(&shared);
    std::printf(" GetSharedHandle=0x%08lx nonnull=%d", (unsigned long)hr, shared != nullptr);
    int failed = FAILED(hr) || !shared;
    if (!failed) {
        ID3D11Texture2D *opened = nullptr;
        hr = consumer->OpenSharedResource(shared, __uuidof(ID3D11Texture2D), (void **)&opened);
        std::printf(" OpenSharedResource=0x%08lx", (unsigned long)hr);
        failed = FAILED(hr);
        if (opened) opened->Release();
    }
    std::puts("");
    resource->Release(); texture->Release();
    return failed;
}
int main(int argc, char **argv) {
    std::setbuf(stdout, nullptr);
    ID3D11Device *a = nullptr, *b = nullptr;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                  nullptr, 0, D3D11_SDK_VERSION, &a, nullptr, nullptr);
    std::printf("CreateDeviceA=0x%08lx\n", (unsigned long)hr);
    if (FAILED(hr)) return 2;
    hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                         nullptr, 0, D3D11_SDK_VERSION, &b, nullptr, nullptr);
    if (FAILED(hr)) { a->Release(); return 3; }
    int errors = 0;
    if (argc == 1 || !std::strcmp(argv[1], "rgba"))
        errors += probe(a, b, DXGI_FORMAT_R8G8B8A8_UNORM, "RGBA8");
    if (argc == 1 || !std::strcmp(argv[1], "nv12"))
        errors += probe(a, b, DXGI_FORMAT_NV12, "NV12");
    b->Release(); a->Release();
    return errors ? 1 : 0;
}
