#include <Windows.h>
#include <wrl.h> // Para Microsoft::WRL::ComPtr
#include <directx/d3d12.h>
#include <dxgi1_4.h>
#include <iostream>
#include <vector> // Necesario para guardar los handles de NVRHI

#include <nvrhi/nvrhi.h>
#include <nvrhi/d3d12.h>

using namespace Microsoft::WRL;

// Función de devolución de llamada para mensajes de Win32
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_ERASEBKGND:
        return 1; // Evita que Windows intente pintar el fondo con GDI (elimina parpadeos)

    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(hwnd, &ps);
        // Aquí no hacemos nada porque DX12 se encarga de dibujar
        EndPaint(hwnd, &ps); // ¡Esto le confirma a Windows que ya terminamos!
        return 0;
    }

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

    // Habilitar la capa de depuración de DX12
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
    }

    ComPtr<IDXGIFactory4> factory;
    CreateDXGIFactory1(IID_PPV_ARGS(&factory));

    ComPtr<ID3D12Device> device;
    D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));

    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    ComPtr<ID3D12CommandQueue> commandQueue;
    device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.BufferCount = 2; // Double buffering
    swapChainDesc.Width = 800;
    swapChainDesc.Height = 600;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    ComPtr<IDXGISwapChain1> swapChain1;
    factory->CreateSwapChainForHwnd(
        commandQueue.Get(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    ComPtr<IDXGISwapChain3> swapChain3;
    swapChain1.As(&swapChain3);
    swapChain3->SetMaximumFrameLatency(1);
    HANDLE frameLatencyWaitEvent = swapChain3->GetFrameLatencyWaitableObject();
    // ---------------------------------------------------------
    // 3. INICIALIZACIÓN DE NVRHI
    // ---------------------------------------------------------
    nvrhi::d3d12::DeviceDesc nvrhiDesc;
    nvrhiDesc.errorCB = &NvrhiMessageCallback::GetInstance();
    nvrhiDesc.pDevice = device.Get();
    nvrhiDesc.pGraphicsCommandQueue = commandQueue.Get();

    nvrhi::DeviceHandle nvrhiDevice = nvrhi::d3d12::createDevice(nvrhiDesc);

    if (!nvrhiDevice) {
        OutputDebugStringA("Error al crear el dispositivo NVRHI\n");
        return -1;
    }

    // --- ¡NUEVO!: CONECTAR SWAPCHAIN CON NVRHI ---
    const int numBackBuffers = 2;
    std::vector<nvrhi::TextureHandle> swapChainTextures(numBackBuffers);

    for (int i = 0; i < numBackBuffers; ++i) {
        ComPtr<ID3D12Resource> backBuffer;
        swapChain1->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

        nvrhi::TextureDesc textureDesc;
        textureDesc.width = 800;
        textureDesc.height = 600;
        textureDesc.format = nvrhi::Format::RGBA8_UNORM;
        textureDesc.isRenderTarget = true;
        textureDesc.initialState = nvrhi::ResourceStates::Present;
        textureDesc.keepInitialState = true;
        textureDesc.debugName = "SwapChainBuffer";

        swapChainTextures[i] = nvrhiDevice->createHandleForNativeTexture(
            nvrhi::ObjectTypes::D3D12_Resource, nvrhi::Object(backBuffer.Get()), textureDesc);
    }

    // Crear la lista de comandos (AFUERA DEL BUCLE)
    nvrhi::CommandListHandle commandList = nvrhiDevice->createCommandList();

    // ---------------------------------------------------------
    // 4. BUCLE DE MENSAJES (MAIN LOOP)
    // ---------------------------------------------------------
    MSG msg = {};
    bool running = true;
    nvrhi::Color clearColor(0.2f, 0.3f, 0.6f, 1.0f); // Azul oscuro

    while (running) {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                running = false;
            }
        }

        if (running) {
            // ¡NUEVO!: Dormir el hilo exactamente hasta que la GPU esté lista
            WaitForSingleObject(frameLatencyWaitEvent, INFINITE);
            // 1. Obtener el buffer actual
            UINT bufferIndex = swapChain1->GetCurrentBackBufferIndex();

            // 2. Abrir comandos y pintar la pantalla
            commandList->open();
            commandList->clearTextureFloat(swapChainTextures[bufferIndex], nvrhi::AllSubresources, clearColor);
            commandList->close();

            // 3. Ejecutar comandos en la GPU
            nvrhiDevice->executeCommandList(commandList);

            // 4. Presentar el frame
            swapChain1->Present(1, 0);
            //swapChain1->Present(0, 0); // El primer '0' desactiva el VSync

            // 5. ¡LA CLAVE DE LOS FPS!: Limpiar la basura del fotograma
            nvrhiDevice->runGarbageCollection();
        }
    }

    // ---------------------------------------------------------
    // 5. LIMPIEZA
    // ---------------------------------------------------------
    swapChainTextures.clear(); // Liberar handles de texturas primero
    commandList = nullptr;     // Liberar comandos
    nvrhiDevice = nullptr;     // Liberar dispositivo

    DestroyWindow(hwnd);
    UnregisterClassW(className, wc.hInstance);

    return 0;
}