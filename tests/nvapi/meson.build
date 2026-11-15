# nvapi64.dll is reached only through nvapi_QueryInterface, so the test needs no NVAPI import library: it loads the
# DLL, resolves ids out of NVIDIA's own table and calls through the pointers. D3D12 because it makes a device to ask
# Reflex about
nvapi_test_deps = [ cpp.find_library('d3d12'), cpp.find_library('dxgi') ]
exe = executable('nvapi_exports', ['nvapi_exports.cpp'], dependencies: nvapi_test_deps,
                 include_directories: include_directories('../../external/nvapi'))
test('nvapi_exports', exe, timeout: 300)
