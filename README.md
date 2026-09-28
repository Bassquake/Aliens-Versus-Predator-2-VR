# Aliens Versus Predator 2: VR
PCVR mod for Aliens Versus Predator 2 (2001) game. This is only for Windows 10 and above. Untested on Linux. Should work on medium graphics cards from Nvidia 2060 and upwards.

> [!CAUTION]
> You need the original installation game files. The 2 disc CD version can be found on eBay or maybe archive.org. This build is in playable state. I'm not primarily a programmer, this was done with a lot of help from various AI like Claude, Copilot and Gemini.

## Installation
- Install the Aliens Versus Predator 2 game using it's default folder location. (C:\Program Files (x86)\Fox\Aliens vs. Predator 2).
- Update it to v1.0.9.6 if it isn't already, use the [Easy Installer from here](https://www.moddb.com/games/aliens-vs-predator-2/downloads/aliens-vs-predator-2-easy-installer).

![Easy Installer](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-easy-installer-step1.jpg)
![Easy Installer](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-easy-installer-step2.jpg)
![Easy Installer](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-easy-installer-step3.jpg)

- May as well install the **SP map update** and **MP map update** too.
- Download the zip in [Releases](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/releases) and extract to a folder somewhere on your drive.
- Go to that folder and right click and Run As Administrator on **install.bat**. This will copy the necessary files into the game folder.
- Before running the VR version we need make sure the display is set to 640x480. Run the **AVP2.exe** in **C:\Program Files (x86)\Fox\Aliens vs. Predator 2**. After a while you'll see a popup:

![AvP2 Options 1](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-options-1.jpg)

- Choose Display. Changing the resolution alters the menus and HUD size. Set it to 640 x 480 x 32.

![AvP2 Options 3](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-options-3.jpg)

- Click OK then PLAY so settings apply. Quit the game and then run **AVP2 VR.bat** in **C:\Program Files (x86)\Fox\Aliens vs. Predator 2** so it runs the VR version properly.

### Tip
To turn off various game settings, run the **AVP2.exe** again in the **C:\Program Files (x86)\Fox\Aliens vs. Predator 2** folder, you will be presented with the popup and choose Options:

![AvP2 Options 2](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/avp2-options-2.jpg)

I like to **Disable Logos** so they don't keep loading every time I start the game.

## Controls
I've tried to map as many of the functions to the buttons. They can be customised by editing the **avp2xr.ini** located in **C:\Program Files (x86)\Fox\Aliens vs. Predator 2**. A number of other settings can be found in there to customise positioning of HUD and weapons etc.

![Control layout](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/controllers-avp2-v1.jpg)

## Enhance the Steam Library
Add the **AVP2 VR.bat** to the Steam Library as a non-Steam game:

![Screenshot of adding non-Steam game](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-app.png)

Rename the shortcut by going to Properties:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-options.png)

Then name the shortcut seen here and set the **Include in VR Library** to on:

![Screenshot of Shortcut naming](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-custom-name.png)

To customise the images in Steam Library so it looks nicer, download the extra zip file **steamvr-custom-images-avp2.zip** in Releases page, unzip the images from steamvr-custom-images-avp2.zip somewhere. Then click the gear icon and select Properties:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-options.png)

Choose Customisation and change images. The image files are named the same as the artwork title:

![Screenshot of Steam options](https://github.com/Bassquake/Aliens-Versus-Predator-2-VR/blob/main/captures/steam-add-images.png)

Finally, to play, on your headset, run Steam Link and navigate to the game in your library. Simply click Play! (Virtual Desktop is untested).
