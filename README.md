# Aliens Versus Predator 2: VR
PCVR mod for Aliens Versus Predator 2 (2001) game. This is for Windows 10 upwards and Steam Link. Tested on Quest 2 and 3. Untested on Linux. **Don't use Meta Link as it doesn't support 32 bit games**. Virtual Desktop should work but untested.

Should work on medium graphics cards from Nvidia 3060 and upwards.

> [!CAUTION]
> You need the original installation game files and updated to v1.0.9.6. The 2 disc CD version can be found on eBay or maybe archive.org. This build is in playable state. Some weapon models needs a bit of attention as they aren't designed to be looked at from different angles! I'm not primarily a programmer, this was done with a lot of help from various AI like Claude, Copilot and Gemini.

## Installation
- Install the Aliens Versus Predator 2 game into its default folder location: **C:\Program Files (x86)\Fox\Aliens vs. Predator 2)**
- Update it to v1.0.9.6 if it isn't already. Update can be [found here](https://www.gamefront.com/games/aliens-vs-predator-2/file/avp2-update-1-0-9-6).
- Download my **avp2-vr-x.x.zip** in [Releases](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/releases) and extract to a folder somewhere on your drive.
- Go to the extracted folder and right click and **Run As Administrator** on **install**. This will copy the necessary files into the game folder, press any key to close the window.

![Install Files](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-install-files-results.jpg)

- Enhance the Steam Library by adding cover art etc:

Add the **AVP2VR_(SteamVR_runtime).bat** to the Steam Library as a non-Steam game:

![Screenshot of adding non-Steam game](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-app.png)

Edit the Properties:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-options.png)

Rename the shortcut seen here, set the Target Path to where **AVP2VR_(SteamVR_runtime).bat** is and set the **Include in VR Library** to on:

![Screenshot of Shortcut naming](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-custom-name.png)

To customise the images in Steam Library so it looks nicer, download the extra zip file **steamvr-custom-images-avp2.zip** in Releases page, unzip the images from steamvr-custom-images-avp2.zip somewhere. Then click the gear icon and select Properties:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-options.png)

Choose Customisation and change images. The image files are named the same as the artwork title:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-images.png)

- Finally, on your headset, run Steam Link and navigate to the game in your library. Simply click Play! (Virtual Desktop is untested). Your desktop will mirror the view.

Note: You can also double click **AVP2VR_(SteamVR_runtime).bat** on the PC and it'll auto load in headset if Steam Link is already running.

## Controls
I've tried to map as many of the functions to the controllers buttons as there are a lot more than Aliens Versus Predator had! They can be customised by editing the **avp2xr.ini** located in **C:\Program Files (x86)\Fox\Aliens vs. Predator 2**.

![Control layout](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/controllers-avp2-v1.jpg)

## Tip
There's many customisation options in **avp2xr.ini** for controls/button mapping and a number of other settings can be found in there to customise positioning of HUD and weapons etc. Too many to list here so read the comments what each one does.
