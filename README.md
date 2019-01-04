# sm6_0
DirectX 12 Shader Model 6.0 compilation test. Running into an issue loading compiled shader blobs on RS4 inspired me to write a simple repro test that also may serve as a reference for developers trying to move to Shader Model 6.0 and the MS DirectXShaderCompiler (https://github.com/Microsoft/DirectXShaderCompiler).

Note, this test requires the following:

 - CMake 3.12 or higher
 - Windows RS4 (Creator's Update) or higher
 - Windows 17763 SDK
