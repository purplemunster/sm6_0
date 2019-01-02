#//============================================================================//
#//                                                                            //
#// This software is distributed under the MIT License (MIT).                  //
#//                                                                            //
#// Copyright (c) 2019 Rohan Mehalwal                                          //
#//                                                                            //
#// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR //
#// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,   //
#// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL    //
#// THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER //
#// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING    //
#// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER        //
#// DEALINGS IN THE SOFTWARE.                                                  //
#//                                                                            //
#//============================================================================//


# Require CMake 3.12 in order to use VS_SHADER* properties for HLSL compilation
cmake_minimum_required (VERSION 3.12)

# Is there a better way of doing this?
set(CompilerFlags
        CMAKE_CXX_FLAGS
        CMAKE_CXX_FLAGS_DEBUG
        CMAKE_CXX_FLAGS_RELEASE
        CMAKE_C_FLAGS
        CMAKE_C_FLAGS_DEBUG
        CMAKE_C_FLAGS_RELEASE
)

# statically link debug runtime
foreach(CompilerFlag ${CompilerFlags})
  string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
endforeach()

# set project name
set(ProjectName "SM6_0")

# define project
project (${ProjectName})

# gather c++ source files
file(GLOB HeaderFiles "*.hpp")
file(GLOB HeaderFiles "*.h")
file(GLOB SourceFiles "*.cpp")

# gather shader files
file(GLOB ShaderFiles "shaders/*.hlsl")

# Common shader properties
set_source_files_properties(${ShaderFiles} PROPERTIES
    VS_SHADER_ENTRYPOINT "main"
    VS_SHADER_MODEL 6.1
    VS_SHADER_OBJECT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/Shaders/%(Filename).cso"
    #VS_SHADER_OUTPUT_HEADER_FILE "${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders/g_%(Filename).h"
)

# setup shader compile
set_source_files_properties("shaders/ComputeShader.hlsl" PROPERTIES VS_SHADER_TYPE Compute)

# setup root signature compile
set_source_files_properties("shaders/RootSignature.hlsl"
    PROPERTIES VS_SHADER_TYPE RootSignature
    VS_SHADER_MODEL           rootsig_1.1
)

# setup executable
add_executable(${ProjectName} ${HeaderFiles} ${SourceFiles} ${ShaderFiles})
target_link_libraries(${ProjectName} dxcompiler d3d12 dxgi)

# install location
install (TARGETS ${ProjectName} DESTINATION bin)