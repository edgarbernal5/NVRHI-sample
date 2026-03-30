#include <Windows.h>
#include <wrl.h> // Para Microsoft::WRL::ComPtr
#include <directx/d3d12.h>
#include <dxgi1_4.h>
#include <iostream>

#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>

using namespace Microsoft::WRL;

// Función de devolución de llamada para mensajes de Win32
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Logger simple para NVRHI
class NvrhiMessageCallback : public nvrhi::IMessageCallback {
public:
    static NvrhiMessageCallback& GetInstance() {
        static NvrhiMessageCallback instance;
        return instance;
    }
    void message(nvrhi::MessageSeverity severity, const char* messageText) override {
        OutputDebugStringA(messageText);
        OutputDebugStringA("\n");
    }
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // ---------------------------------------------------------
    // 1. INICIALIZACIÓN DE WIN32
    // ---------------------------------------------------------
    const wchar_t* className = L"NVRHI_DX12_Class";
    WNDCLASSEXW wc = { sizeof(WNDCLASSEXW), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, className, NULL };
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowExW(0, className, L"NVRHI + DX12 + Win32", 
                                WS_OVERLAPPEDWINDOW, 100, 100, 800, 600, 
                                NULL, NULL, wc.hInstance, NULL);
    
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ---------------------------------------------------------
    // 2. INICIALIZACIÓN DE DIRECTX 12
    // ---------------------------------------------------------
    
    // Habilitar la capa de depuración de DX12 (Recomendado en desarrollo)
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }

    // Crear DXGI Factory
    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    // Crear D3D12 Device
    ComPtr<ID3D12Device> device;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

    // Crear Command Queue (NVRHI lo necesita)
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ComPtr<ID3D12CommandQueue> commandQueue;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

    // Crear Swap Chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2; // Double buffering
    swapChainDesc.Width = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;

    ComPtr<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(
        commandQueue.Get(), // El swap chain necesita el queue en DX12
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    // ---------------------------------------------------------
    // 3. INICIALIZACIÓN DE NVRHI
    // ---------------------------------------------------------
    nvrhi::d3d12::DeviceDesc nvrhiDesc;
    nvrhiDesc.errorCB = &NvrhiMessageCallback::GetInstance();
    nvrhiDesc.pDevice = device.Get();
    nvrhiDesc.pGraphicsCommandQueue = commandQueue.Get();

    // Crear la instancia principal de NVRHI
    nvrhi::DeviceHandle nvrhiDevice = nvrhi::d3d12::createDevice(nvrhiDesc);

    if (!nvrhiDevice) {
        OutputDebugStringA("Error al crear el dispositivo NVRHI\n");
        return -1;
    }

    // ---------------------------------------------------------
    // 4. BUCLE DE MENSAJES (MAIN LOOP)
    // ---------------------------------------------------------
    MSG msg = {};
    bool running = true;
    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }

        if (running) {
            // Aquí irá el código de renderizado (ClearScreen, Draw, etc.)
            
            // Presentar el frame
            swapChain1->Present(1, 0);
        }
    }

    // ---------------------------------------------------------
    // 5. LIMPIEZA
    // ---------------------------------------------------------
    nvrhiDevice = nullptr; // Se libera automáticamente
    DestroyWindow(hwnd);
    UnregisterClassW(className, wc.hInstance);

    return 0;
}