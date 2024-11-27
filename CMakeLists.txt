cmake_minimum_required(VERSION 3.10)

project (BlitzenEngine VERSION 0)

find_package (Vulkan REQUIRED COMPONENTS glslc)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED True)

add_executable(BlitzenEngine
                src/mainEngine.cpp
                src/mainEngine.h

                src/BlitzenVulkan/vulkanRenderer.h
                src/BlitzenVulkan/vulkanRenderer.cpp
                src/BlitzenVulkan/vulkanData.cpp
                src/BlitzenVulkan/vulkanData.h
                src/BlitzenVulkan/vulkanPipelines.h
                src/BlitzenVulkan/vulkanPipelines.cpp
                src/BlitzenVulkan/vulkanInit.cpp

                src/Game/camera.cpp
                src/Game/camera.h

                src/Input/controller.h
                src/Input/controller.cpp

                src/Core/math.h
                src/Core/blitLogger.h
                src/Core/blitzenLogger.cpp
                src/Core/blitAssert.h
                
                #External dependency source code that needs to be include for some libararies to work
                ExternalDependencies/fastgltf/src/fastgltf.cpp
                ExternalDependencies/fastgltf/src/base64.cpp
                ExternalDependencies/fastgltf/src/simdjson.cpp)

add_subdirectory(ExternalDependencies/glfw)

target_link_directories(BlitzenEngine PUBLIC
                        "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Lib")

target_link_libraries(BlitzenEngine PUBLIC 
                        glfw 
                        vulkan-1)

target_include_directories(BlitzenEngine PUBLIC
                    "${PROJECT_SOURCE_DIR}/src"
                    "${PROJECT_SOURCE_DIR}/ExternalDependencies/glfw/include"
                    "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/include"
                    "${PROJECT_SOURCE_DIR}/ExternalDependencies/fastgltf/include"
                    "${PROJECT_SOURCE_DIR}/ExternalDependencies/STBImage")



#Copy assets folder
add_custom_target(copy_assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_LIST_DIR}/Assets ${CMAKE_CURRENT_BINARY_DIR}/Assets
)
add_dependencies(BlitzenEngine copy_assets)


                    
#---------------------------------------------------------------------------------------------------
#Compiling Shaders
#

find_program(GLSL_VALIDATOR glslangValidator HINTS $"{PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Bin")

set(GLSL_VALIDATOR "${PROJECT_SOURCE_DIR}/ExternalDependencies/Vulkan/Bin/glslangValidator.exe")
  
  
  file(GLOB_RECURSE GLSL_SOURCE_FILES
      "VulkanShaders/*.glsl"
      )
  
  foreach(GLSL ${GLSL_SOURCE_FILES})
    get_filename_component(FILE_NAME ${GLSL} NAME)
    set(SPIRV "${PROJECT_BINARY_DIR}/VulkanShaders/${FILE_NAME}.spv")
    add_custom_command(
      OUTPUT ${SPIRV}
      COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/VulkanShaders/"
      COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
      DEPENDS ${GLSL})
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
  endforeach(GLSL)
  
  add_custom_target(
      VulkanShaders 
      DEPENDS ${SPIRV_BINARY_FILES}
      )
  
  add_dependencies(BlitzenEngine VulkanShaders)
  
  add_custom_command(TARGET BlitzenEngine POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:BlitzenEngine>/VulkanShaders/"
      COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${PROJECT_BINARY_DIR}/VulkanShaders"
          "$<TARGET_FILE_DIR:BlitzenEngine>/VulkanShaders"
          )