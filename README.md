Blitzen Engine is a barebones game engine primarily focused on rendering large 3D scenes using Vulkan (and hopefully DX12 as well in the future). 
The 0.X version started off from the same point that the 0.Alpha left off (that version is hosted in a different repository, with bindless branch being the most functional one), 
but it now compiles as an excecutable instead of a static library. 

It is now able to load and draw static gltf scenes like the 0.Alpha version, but it uses bindless resources for everything not just the vertex and index buffer. It also has 2 pipelines that can be switched on the fly.
One uses the traditional vkCmdDrawIndexed with a for loop and uses push constants to access all bindless resources. The other uses one call to vkCmdDrawIndexedIndirect and accesses everything from with the shader.
If BLITZEN_START_VULKAN_WITH_INDIRECT macro is defined as true, Blitzen can switch between the 2 modes by pressing tab. The next step is to add frustum culling to both pipelines. The indirect version will ready the commands 
after doing frustum culling on a compute shader, while the other one might have to do it on the CPU. Eventually LODs and occlusion culling will be added which will complete the immediate goals of this version of the engine.


As of now the engine does not support most platforms. It can be built and compiled on any hardware that has cmake, vulkan (and the GPU supports it) and Msvc. Everything else seems to fail, either because of some STL libraries
that some of the external dependecies use or because cmake cannot find some packages that it wants for Vulkan (this has only happened on Linux). The build might also fail at runtime, since I have not properly set the application 
to not use newer extensions when it runs on older hardware.

Fixing these issues is not my priotrity right now, since the engine still has alot of room to grow on the 3D graphics side. Besides, I doubt anyone will see this anyway.
