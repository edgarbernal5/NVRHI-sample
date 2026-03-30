cmake_minimum_required(VERSION 3.15)
project(NvrhiWin32DX12 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Forzar las variables en el scope actual (sin CACHE)
set(NVRHI_WITH_D3D12 ON)
set(NVRHI_WITH_VULKAN OFF)
set(NVRHI_WITH_D3D11 OFF)

# Incluir NVRHI
add_subdirectory(external/nvrhi)

# Crear el ejecutable
add_executable(HelloWorld WIN32 main.cpp) # WIN32 oculta la consola por defecto

# Vincular NVRHI, sus backends y las bibliotecas de DirectX/Windows
target_link_libraries(HelloWorld PRIVATE 
    nvrhi
    nvrhi_d3d12
    DirectX-Headers
    d3d12.lib
    dxgi.lib
    d3dcompiler.lib
)