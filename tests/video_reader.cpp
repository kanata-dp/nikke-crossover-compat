/* SPDX-License-Identifier: LGPL-2.1-or-later */
#include <windows.h>
#include <d3d11.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <cstdio>

#define CHECK(call) do { HRESULT r = (call); if (FAILED(r)) { \
    std::printf("%s: 0x%08lx\n", #call, (unsigned long)r); return 2; } } while (0)

int wmain(int argc, wchar_t **argv)
{
    std::setbuf(stdout, nullptr);
    if (argc != 2 && argc != 3) return 1;
    CHECK(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
    CHECK(MFStartup(MF_VERSION));
    ID3D11Device *device = nullptr;
    CHECK(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        nullptr, 0, D3D11_SDK_VERSION, &device, nullptr, nullptr));
    IMFDXGIDeviceManager *manager = nullptr;
    UINT reset;
    HRESULT manager_result = MFCreateDXGIDeviceManager(&reset, &manager);
    std::printf("MFCreateDXGIDeviceManager=0x%08lx\n", (unsigned long)manager_result);
    if (FAILED(manager_result) && argc != 3) return 2;
    if (manager) CHECK(manager->ResetDevice(device, reset));
    IMFAttributes *attributes = nullptr;
    CHECK(MFCreateAttributes(&attributes, 3));
    if (manager) CHECK(attributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, manager));
    CHECK(attributes->SetUINT32(MF_SA_D3D11_SHARED_WITHOUT_MUTEX, TRUE));
    IMFSourceReader *reader = nullptr;
    CHECK(MFCreateSourceReaderFromURL(argv[1], attributes, &reader));
    IMFMediaType *type = nullptr;
    CHECK(MFCreateMediaType(&type));
    CHECK(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    CHECK(type->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12));
    CHECK(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, type));
    unsigned frames = 0, gpu_frames = 0, share_failures = 0;
    unsigned long long checksum = 0, first_checksum = 0;
    bool changed = false;
    while (frames < 60)
    {
        DWORD flags = 0;
        IMFSample *sample = nullptr;
        CHECK(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, nullptr, &flags, nullptr, &sample));
        if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!sample) continue;
        IMFMediaBuffer *buffer = nullptr;
        CHECK(sample->GetBufferByIndex(0, &buffer));
        IMFDXGIBuffer *dxgi = nullptr;
        if (SUCCEEDED(buffer->QueryInterface(IID_PPV_ARGS(&dxgi))))
        {
            ++gpu_frames;
            IDXGIResource *resource = nullptr;
            CHECK(dxgi->GetResource(IID_PPV_ARGS(&resource)));
            HANDLE shared = nullptr;
            if (FAILED(resource->GetSharedHandle(&shared)) || !shared) ++share_failures;
            resource->Release();
            dxgi->Release();
        }
        BYTE *data = nullptr;
        DWORD length = 0;
        CHECK(buffer->Lock(&data, nullptr, &length));
        unsigned long long sum = 0;
        for (DWORD i = 0; i < length; ++i) sum += data[i];
        if (!frames) first_checksum = sum;
        else if (sum != first_checksum) changed = true;
        checksum += sum;
        CHECK(buffer->Unlock());
        buffer->Release(); sample->Release();
        ++frames;
    }
    std::printf("frames=%u gpu_frames=%u share_failures=%u changed=%d checksum=%llu\n",
                frames, gpu_frames, share_failures, changed, checksum);
    type->Release(); reader->Release(); attributes->Release();
    if (manager) manager->Release();
    device->Release();
    MFShutdown(); CoUninitialize();
    return frames >= 20 && changed && checksum ? 0 : 3;
}
