# Debugging the Fallback Layer with PIX
[DOWNLOAD HERE](https://blogs.msdn.microsoft.com/pix/download/)

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
