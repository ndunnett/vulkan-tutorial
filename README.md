Basic template using CMake to build a project for completing the [Vulkan Tutorial](https://vulkan-tutorial.com/). Install the Vulkan SDK, clone this repo, then start the tutorial at "Drawing a triangle" using your preferred IDE ie. VSCode. There is one test set up using doctest to check that everything works, much in the same way that the tutorial does in the "development environment" section.

All dependencies are configured with CMake. As long as you have the Vulkan SDK and CMake installed and set up correctly, it should just work™. The contents of the "assets" directory (ie. textures, models, shaders, etc.) will be copied into the directory containing the executable during build time.

See the "completed" branch for the completed tutorial (though it did not start from this template exactly) or the "rewrite" branch for my refactored version of the tutorial using Vulkan-Hpp.
