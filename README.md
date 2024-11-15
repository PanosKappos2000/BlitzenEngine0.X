Blitzen Engine is a barebones game engine primarily focused on rendering large 3D scenes using Vulkan (and hopefully DX12 as well in the future). 
The 0.X version starts off from the same point that the 0.Alpha left off (that version is hosted in a different repository, with bindless branch being the most functional one), 
but it is now an executable instead of a static library. The idea is that if a game was to be made with this engine, the game would be the static library (though it will probably never reach this point).

This version focuses on putting most of the rendering work over to the GPU, starting off with meshes and then making textures and materials bindless.
The bindless edition of the 0.Alpha throws all scene vertices and indices into a unified buffer, which is a good start. Now the renderer should call the indirect versions of the drawing functions, to draw everything with one call.
The plan is to also use compute shaders to ready the indirect commands and do frustrum culling. Once all this is done, hopefully we can start loading textures in a similar bindless approach.
This will free up the GPU to hopefully do some physics calculations in future version that add physics.



If someone tries to test their project on their machine (unlikely that anyone will ever see this), it might not work. I have tested this on other machines and it failed to build on an ubuntu desktop and failed to compile
on a windows laptop that used gcc instead of Msvc. It did succeed on a windows laptop that used Msvc and created a visual studio solution with cmake. The code compiled and the scene rendered normally (0.Alpha version).
If someone ever wants to try for whatever reason, running cmake should be enough. Then the scene that is loaded can be changed by going to the Init function of the VulkanRenderer class and changing the filepath in the LoadScene call.
It should be a .gltf or .glb file and if it does not require some extensions that the gltf loading library I use does not support, it should render normally and the user can move the camera with mouse and WASD keys

It's also important to note that this version has textures, materials and colors stripped from the shaders and the draw loop (it still loads them but they're never used). A more "complete" version would be the 0.Alpha which renders 
the scenes with textures and has actually been tested on a different machine. If one uses that version, to load different scenes, they would have to change the filepath in mainEngine.cpp on the LoadScene function called from Init(().
BlitzenEngine0.Alpha is hosted on the repository of the same name on my account.

Currently, I have no plans on fixing this issue as this project serves more as a way to understand 3D graphics programming. If it becomes a worthwhile application in the far future, I will see what I can do about its portability issues.

The fastgltf library I use is this one https://github.com/spnda/fastgltf (I use an older version than the current)
