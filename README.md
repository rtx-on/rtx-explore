# RTX-Explore - DXR Path Tracer
<p align="center">
  <kbd>
  <img src="https://github.com/rtx-on/rtx-explore/blob/master/Images/final/coffee_front.bmp"/>
   </kbd>
</p>
<p align="center">
  <kbd>
  <img src="https://github.com/rtx-on/rtx-explore/blob/master/Images/final/coffee_right_pan.bmp"/>
    </kbd>
</p>

# Project Goal
With this project, our group planned to leverage the power of Nvidia's & Microsoft's new DirectX Raytracing platform to implement a fast Path Tracer. **Our code does NOT require an actual RTX GPU**. A Shader Model 6.0 (sm 6.0) compatible GPU is enough, given that you import the [fallback layer](https://github.com/rtx-on/rtx-explore/blob/master/README.md#building--running).

# Blog Posts for Detailed Explanations
- [Introduction](https://maknee.github.io/dxr/2018/12/06/RTX-DXR-Path-Tracer/)
- [User Guide](https://maknee.github.io/dxr/2018/12/06/RTX-DXR-Path-Tracer-User-Guide/)
- [Host Code Explanation](https://maknee.github.io/dxr/2018/12/07/RTX-DXR-Path-Tracer-Host/)
- [HLSL Shader Code Explanation](https://maknee.github.io/dxr/2018/12/08/RTX-DXR-Path-Tracer-HLSL/)


# Path Tracing Intro
<p align="center">
  <img width="300" height="400" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/explanation.png"/>
</p>

Path Tracing is a computer graphics Monte Carlo method of rendering images of three-dimensional scenes such that the global illumination is faithful to reality. In simple terms, a path tracer fires rays from each pixel, which would bounce off in many directions depending on the objects in the scene. If the ray is fortunate enough, it would hit an illuminating surface (lightbulb, sun, etc...), and would cause the first object it hit to be illuminated, much like how light travels towards our eyes.

This faithfulness to the physical properties of light allows path tracing to generate significantly more photorealistic images. The difference is particularly noticeable in reflective and refractive surfaces

# Features
## Dynamic Loading of Models
### .glTF
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 
### .OBJ
<p align="center">
  <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/ms2/scene_loading.png"/>
  </kbd>
</p> 

## Dynamic Loading of Textures & Materials
### Textures
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 
### Materials
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

## Scene Transformation Tools
<p align="center">
    <kbd>
  <img src="https://github.com/rtx-on/rtx-explore/blob/master/Images/ms3/good%20dargon.PNG"/>
  </kbd>
</p> 

## Support for diffuse, reflective, and refractive materials
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/ms3/bloch.PNG"/>
  </kbd>
</p> 

## Different Materials Support
### Diffuse
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

### Specular
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

### Dispersive
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

### Transmissive
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

## Normal Mapping
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

## Subsurface Scattering
### Low Scattering
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

### High Scattering
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

## Image Comparison
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

## Special Effects
### Anti-Aliasing
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

### Depth of Field
<p align="center">
    <kbd>
  <img width="700" height="350" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/gun_normap.gif"/>
  </kbd>
</p> 

# Required Build Environment
* Shader Model 6.0 support on GPU
* Visual Studio 2017 version 15.8.4 or higher.
* [Windows 10 October 2018 (17763) SDK](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk)
     * Get the ISO
     * Mount
     * Install all options preferably
* Developer mode enabled on Windows 10

# Building & Running
1) Test that you can run all 3 samples
   * Go to /src
   * Choose one of the sample (HelloTriangle, Procedural, SimpleLighting) and Set as StartUp Project
   * Build and run in Debug/Release mode
      * Make sure that the NuGet package manager can automatically retrieve missing packages. This might require running build twice.
2) Test that you can run the Path Tracer
   * Make sure that you pass a valid scene file to as an argument
      * Properties > Debugging > Command Line Arguments > "src/scenes/cornell.txt"

# Debugging the Fallback Layer with PIX
[https://blogs.msdn.microsoft.com/pix/download/](https://blogs.msdn.microsoft.com/pix/download/)

1) Go to the downlaod page and click to download the latest version (in the image below, PIX-1810.24)

<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_download.png"/>
  </kbd>
</p> 

2) Run the executable and something like this should show up:
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_front.png"/>
  </kbd>
</p> 

3) Go to `Launch Win32` tab under Select Target Process
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_select_target.png"/>
  </kbd>
</p> 

4) Find the folder that is rtx-explore
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_base_folder.png"/>
  </kbd>
</p> 

5) In the `Working Directory` line, fill in the line with the rtx-explorer location + `\src\D3D12PathTracer`. Example:`C:\Users\user\Desktop\RTX\rtx-explore\src\D3D12PathTracer`

<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_working_dir.png"/>
  </kbd>
</p> 

6) In the `Path to Executable` line, fill in the line with the rtx-explorer location + `\src\D3D12PathTracer\bin\x64\Debug\D3D12PathTracer.exe`. Example: `C:\Users\user\Desktop\RTX\rtx-explore\src\D3D12PathTracer\bin\x64\Debug\D3D12PathTracer.exe`

<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_path_to_executable.png"/>
  </kbd>
</p> 

7) Press Launch, and the application should show up
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_show.png"/>
  </kbd>
</p> 

8) Now you can debug! Press `Print screen` button to capture a frame and `1 captures taken` will appear under `GPU Capture`
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_print_screen.png"/>
  </kbd>
</p> 


9) Double click the image under `1 captures taken` Then, a screen will be brought up like below:
<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_page.png"/>
  </kbd>
</p> 

10) Press the `play` button after `Analysis is not running.` at the top right to begin analysis of the frame. You should get `Analysis is running on NVIDIA ...`, in our case `NVIDIA GeForce GTX 1070`

<p align="center">
    <kbd>
  <img width="400" height="250" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/debugging/pix_analysis.png"/>
  </kbd>
</p> 

**For more tips on how to debug, see:** [playlist](https://www.youtube.com/playlist?list=PLeHvwXyqearWuPPxh6T03iwX-McPG5LkB)

## Credits
### Titan V GPU
The Titan V used for this project was donated by the NVIDIA Corporation.
We gratefully acknowledge the support of NVIDIA Corporation with the donation of the Titan V GPU used for this project.



