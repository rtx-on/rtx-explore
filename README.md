# RTX-Explore - DXR Path Tracer

# Project Goal
<p align="center">
  <img src="https://github.com/rtx-on/rtx-explore/blob/master/Images/gifs/pathtracer.gif"/>
</p>

Our group implemented a Path Tracer that runs on Nvidia's & Microsoft's DirectX Raytracing platform. **Our code does NOT require an actual RTX GPU**. A Shader Model 6.0 (sm 6.0) compatible GPU is enough, given that you import the [fallback layer]().

# Path Tracing Intro
<p align="center">
  <img width="300" height="400" src="https://github.com/rtx-on/rtx-explore/blob/master/Images/explanation.png"/>
</p>

Path Tracing is a computer graphics Monte Carlo method of rendering images of three-dimensional scenes such that the global illumination is faithful to reality. In simple terms, a path tracer fires rays from each pixel, which would bounce off in many directions depending on the objects in the scene. If the ray is fortunate enough, it would hit an illuminating surface (lightbulb, sun, etc...), and would cause the first object it hit to be illuminated, much like how light travels towards our eyes.

# Required Build Environment
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
2) TBD

# Debugging the Fallback Layer

### Option 1: Microsoft Pix debugger

[https://blogs.msdn.microsoft.com/pix/download/](https://blogs.msdn.microsoft.com/pix/download/)

Go to the downlaod page and click to download the latest version (in the image below, PIX-1810.24)

![](Images/debugging/pix_download.png)

Now, once you have it downloaded, run the executable and something like this should show up:

![](Images/debugging/pix_front.png)

Go to `Launch Win32` tab under Select Target Process

![](Images/debugging/pix_select_target.png)

Find the folder that is rtx-explore

![](Images/debugging/pix_base_folder.png)

In the `Working Directory` line, fill in the line with the rtx-explorer location + `\src\D3D12PathTracer`

`C:\Users\SIG\Desktop\RTX\rtx-explore\src\D3D12PathTracer`

![](Images/debugging/pix_working_dir.png)

In the `Path to Executable` line, fill in the line with the rtx-explorer location + `\src\D3D12PathTracer\bin\x64\Debug\D3D12PathTracer.exe`

`C:\Users\SIG\Desktop\RTX\rtx-explore\src\D3D12PathTracer\bin\x64\Debug\D3D12PathTracer.exe`

![](Images/debugging/pix_path_to_executable.png)

Press Launch, and the application should show up

![](Images/debugging/pix_show.png)

Now, it's time to mess with Pix!

Press `Print screen` button to capture a frame and `1 captures taken` will appear under `GPU Capture`

![](Images/debugging/pix_print_screen.png)

Double click the image under `1 captures taken`

![](Images/debugging/pix_capture.png)

Then, a screen will be brought up like below:

![](Images/debugging/pix_page.png)

Press the `play` button after `Analysis is not running.` at the top right to begin analysis of the frame 

![](Images/debugging/pix_analysis.png)

You should get `Analysis is running on NVIDIA ...`, in my case `NVIDIA GeForce GTX 1070`

![](Images/debugging/pix_analysis_finished.png)

Now, mess around with pix and take a look at this quick playlist to learn how to debug with PIX 

[Playlist link](https://www.youtube.com/playlist?list=PLeHvwXyqearWuPPxh6T03iwX-McPG5LkB)

### Option 2: Nvidia debugger (RTX only)

[https://developer.nvidia.com/nsight-visual-studio-edition](https://developer.nvidia.com/nsight-visual-studio-edition)


