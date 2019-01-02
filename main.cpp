//============================================================================//
//                                                                            //
// This software is distributed under the MIT License (MIT).                  //
//                                                                            //
// Copyright (c) 2019 Rohan Mehalwal                                          //
//                                                                            //
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
// DEALINGS IN THE SOFTWARE.                                                  //
//                                                                            //
//============================================================================//

// Test sample compiling shader model 6.0 shaders
//
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <stdexcept>

#include "dxcapi.use.h"

//=====================================================================================================================
namespace dx12
{
// core d3d12
Microsoft::WRL::ComPtr<IDXGIFactory2>   factory;    ///< DXGI factory
Microsoft::WRL::ComPtr<ID3D12Device>    device;     ///< D3D12 device
Microsoft::WRL::ComPtr<ID3D12InfoQueue> info;       ///< D3D12 debug info queue

// compiler
dxc::DxcDllSupport                    dll;          ///< dxcompiler DLL helper
Microsoft::WRL::ComPtr<IDxcCompiler>  compiler;     ///< DXC compiler instance
Microsoft::WRL::ComPtr<IDxcLibrary>   library;      ///< DXC library instance
Microsoft::WRL::ComPtr<IDxcValidator> validator;    ///< DXIL validator instance

//=====================================================================================================================
// Helper function for validating D3D return codes
inline void ThrowIfError(
    HRESULT hr, const char* pMessage)
{
    if (FAILED(hr))
    {
        char buffer[256];
        sprintf_s(buffer, "Error: %s", pMessage);
        throw std::runtime_error(buffer);
    }
}

//=====================================================================================================================
// Initialise D3D12
void Init(uint32_t gpuIndex)
{
#if _DEBUG
    Microsoft::WRL::ComPtr<ID3D12Debug> debug = { nullptr };
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug))))
    {
        debug->EnableDebugLayer();
    }
#endif

    // create factory
    auto hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    ThrowIfError(hr, "failed to create DXGI factory");

    std::printf("Enumerating adapters.. \n");
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter = { nullptr };
    if (gpuIndex == ~0)
    {
        // pick the first adapter
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter = { nullptr };
        for (uint32_t idx = 0; SUCCEEDED(factory->EnumAdapters1(idx, &adapter)); ++idx)
        {
            DXGI_ADAPTER_DESC1 desc = {};
            auto hr = adapter->GetDesc1(&desc);
            ThrowIfError(hr, "failed to query adapter description");

            std::printf("\nAdapter [%d] => %S \n", idx, desc.Description);

            // skip software adapter
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                continue;
            }

            // filter adapters that do not support dx12
            if (SUCCEEDED(D3D12CreateDevice(
                adapter.Get(), D3D_FEATURE_LEVEL_11_1, _uuidof(ID3D12Device), nullptr)))
            {
                gpuIndex = idx;
                break;
            }
        }

        // make sure we have an adapter by now..
        if (adapter = nullptr)
        {
            ThrowIfError(E_FAIL, "Failed to find dx12 compatible adapters");
        }
    }
    else
    {
        // enumerate caller specified adapter
        hr = factory->EnumAdapters1(gpuIndex, &adapter);
        ThrowIfError(hr, "failed to enumerate adapter. Invalid adapter index");
    }

    std::printf("\nCreating D3D12 device using adapter %d", gpuIndex);
    hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
    ThrowIfError(hr, "failed to create device");

    // Check if we are running on Windows RS5 OS
    Microsoft::WRL::ComPtr<ID3D12Device5> device5 = { nullptr };
    hr = dx12::device->QueryInterface(IID_PPV_ARGS(&device5));

    // If we failed here, check if we are running on RS4 OS
    if (FAILED(hr))
    {
        // Note that Windows 10 Creator Update SDK is required for enabling Shader Model 6 feature.
        Microsoft::WRL::ComPtr<ID3D12Device4> device4 = { nullptr };
        hr = dx12::device->QueryInterface(IID_PPV_ARGS(&device4));
        ThrowIfError(hr, "failed to acquire RS4 device. Make sure you are running on Windows RS4+ OS");

        std::printf("\nRunning on Windows Redstone 4+ OS");

        // Shader Model 6.0 support is experimental on RS4 and has to be explicitly enabled
        hr = D3D12EnableExperimentalFeatures(1, &D3D12ExperimentalShaderModels, nullptr, nullptr);
        ThrowIfError(hr, "failed to enable experimental features. Make sure the adapter selected supports SM6.0");

        std::printf("\nEnabled experimental shader models");

        // re-create device with SM6.0 features enabled
        hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(device.ReleaseAndGetAddressOf()));
        ThrowIfError(hr, "failed to create device with SM6.0 enabled");
    }
    else
    {
        std::printf("\nRunning on Windows Redstone 5+ OS");
    }

    // Check highest supported shader model
    D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = { D3D_SHADER_MODEL_6_0 };
    hr = device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel));
    ThrowIfError(hr, "failed to check sm6.0 feature support");

    uint32_t minVersion = static_cast<uint32_t>(shaderModel.HighestShaderModel) & 0xF;
    uint32_t majVersion = (static_cast<uint32_t>(shaderModel.HighestShaderModel) >> 4) & 0xF;
    printf("\nHighest supported shader model version: SM %d.%d", majVersion, minVersion);

    // acquire debug device
    hr = device->QueryInterface(IID_PPV_ARGS(&info));
    if (FAILED(hr))
    {
        printf("\nMissing debug SDK. Debug spew may be missing!");
    }

    // initialise compiler
    hr = dll.Initialize();
    ThrowIfError(hr, "failed to initialise DXC compiler DLL");

    hr = dll.CreateInstance(CLSID_DxcCompiler, compiler.GetAddressOf());
    ThrowIfError(hr, "failed to initialise DXC compiler instance");

    hr = dll.CreateInstance(CLSID_DxcLibrary, library.GetAddressOf());
    ThrowIfError(hr, "failed to initialise DXC library instance");

    hr = dll.CreateInstance(CLSID_DxcValidator, validator.GetAddressOf());
    ThrowIfError(hr, "failed to initialise DXIL validator instance");
}

//=====================================================================================================================
/// Use this type to describe a DXIL container of parts.
struct DxilContainerHeader
{
    uint32_t HeaderFourCC;
    uint32_t HashDigest[4];
};

//=====================================================================================================================
inline bool IsDxilSigned(void* buffer)
{
    DxilContainerHeader* pHeader = reinterpret_cast<DxilContainerHeader*>(buffer);

    bool has_digest = false;
    has_digest |= (pHeader->HashDigest[0] != 0x0);
    has_digest |= (pHeader->HashDigest[1] != 0x0);
    has_digest |= (pHeader->HashDigest[2] != 0x0);
    has_digest |= (pHeader->HashDigest[3] != 0x0);

    return has_digest;
}

//=====================================================================================================================
Microsoft::WRL::ComPtr<IDxcBlobEncoding> CreateBlobFromFile(const wchar_t* pFileName)
{
    uint32_t codePage = 0;

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> encoding = nullptr;
    dx12::library->CreateBlobFromFile(pFileName, &codePage, &encoding);

    return encoding;
}

//=====================================================================================================================
Microsoft::WRL::ComPtr<IDxcBlobEncoding> LoadBinary(const wchar_t* pFileName)
{
    uint32_t codePage = 0;

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> encoding = nullptr;
    dx12::library->CreateBlobFromFile(pFileName, &codePage, &encoding);

    Microsoft::WRL::ComPtr<IDxcBlob> blob;
    bool isSigned = IsDxilSigned(encoding->GetBufferPointer());
    std::printf("\nDXIL signing status: %s", isSigned ? "true" : "false");

    if (isSigned == false)
    {
        // Sign DXIL
        Microsoft::WRL::ComPtr<IDxcOperationResult> result = nullptr;
        auto hr = validator->Validate(encoding.Get(), DxcValidatorFlags_InPlaceEdit, result.GetAddressOf());

        HRESULT status;
        result->GetStatus(&status);
        ThrowIfError(hr, "validation failed");
    }

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> dism = nullptr;
    auto hr = dx12::compiler->Disassemble(encoding.Get(), dism.GetAddressOf());
    ThrowIfError(hr, "disassembly failed");

    printf("%s", std::string((char*)dism->GetBufferPointer()).c_str());

    return encoding;
}

//=====================================================================================================================
inline void ReportError(
    IDxcOperationResult* pResult)
{
    IDxcBlobEncoding* pError = nullptr;
    HRESULT status = pResult->GetErrorBuffer(&pError);
    if (SUCCEEDED(status))
    {
        BOOL known = FALSE;
        uint32_t codepage = 0;
        status = pError->GetEncoding(&known, &codepage);
        if (SUCCEEDED(status))
        {
            throw std::runtime_error(std::string((char*)pError->GetBufferPointer()));
        }
    }
}

//=====================================================================================================================
Microsoft::WRL::ComPtr<IDxcBlob> CompileShaderFromFile(
    const wchar_t* pFile, const wchar_t* pEntry, const wchar_t* pProfile)
{
    uint32_t codePage = 0;

    Microsoft::WRL::ComPtr<IDxcBlobEncoding> source = { nullptr };
    auto hr = dx12::library->CreateBlobFromFile(pFile, &codePage, source.GetAddressOf());
    ThrowIfError(hr, "error reading HLSL file");

    Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler = { nullptr };
    hr = dx12::library->CreateIncludeHandler(&includeHandler);
    ThrowIfError(hr, "failed to create include handler");

    const wchar_t* args[] =
    {
        L"/O3"
    };

    Microsoft::WRL::ComPtr<IDxcOperationResult> result = { nullptr };
    hr = dx12::compiler->Compile(source.Get(),
                                 pFile,
                                 pEntry,
                                 pProfile,
                                 args, _countof(args),
                                 nullptr, 0,
                                 includeHandler.Get(),
                                 &result);
    ThrowIfError(hr, "failed to compile HLSL");

    HRESULT status = S_OK;
    hr = result->GetStatus(&status);
    ThrowIfError(hr, "failed to get result status");

    if (FAILED(status))
    {
        ReportError(result.Get());
    }

    Microsoft::WRL::ComPtr<IDxcBlob> blob = nullptr;
    hr = result->GetResult(blob.GetAddressOf());
    ThrowIfError(hr, "failed to get result buffer");

    return blob;
}

//=====================================================================================================================
inline Microsoft::WRL::ComPtr<ID3D12RootSignature>
    CreateRootSignature(Microsoft::WRL::ComPtr<IDxcBlobEncoding> const& grs)
{
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rso = { nullptr };
    auto hr = dx12::device->CreateRootSignature(0, grs->GetBufferPointer(), grs->GetBufferSize(), IID_PPV_ARGS(&rso));
    ThrowIfError(hr, "failed to create global root signature");

    return rso;
}

} // namespace dx12

//=====================================================================================================================
int main(int argc, char* argv[])
{
    uint32_t gpuIndex = ~0; // pick default gpu

    dx12::Init(gpuIndex);

    auto cs = dx12::LoadBinary(L"shaders//ComputeShader.cso");
    auto rs = dx12::CreateBlobFromFile(L"shaders//RootSignature.cso");
    auto rootSignature = dx12::CreateRootSignature(rs);

    D3D12_COMPUTE_PIPELINE_STATE_DESC csPso = {};
    csPso.CS.pShaderBytecode = cs->GetBufferPointer();
    csPso.CS.BytecodeLength  = cs->GetBufferSize();
    csPso.pRootSignature     = rootSignature.Get();

    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline = { nullptr };
    auto hr = dx12::device->CreateComputePipelineState(&csPso, IID_PPV_ARGS(&pipeline));

    if (FAILED(hr))
    {
        printf("\nFailed to compile pipeline!");

        const size_t msgCount = dx12::info->GetNumStoredMessages();
        for (size_t i = 0; i < msgCount; ++i)
        {
            // Get the size of the message
            size_t messageLength = 0;
            HRESULT hr = dx12::info->GetMessage(i, NULL, &messageLength);

            // Allocate space and get the message
            D3D12_MESSAGE* pMessage = static_cast<D3D12_MESSAGE*>(malloc(messageLength));
            hr = dx12::info->GetMessage(i, pMessage, &messageLength);

            printf("\n [%d] -- %s", static_cast<int>(i), pMessage->pDescription);

            // free allocated space
            free(pMessage);
        }
    }

    return 0;
}