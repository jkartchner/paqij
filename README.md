# Paqij Game Engine 

The paqij game engine is for an educational game for children. This game engine will use no libraries, though it currently uses the C standard library to speed development. Currently, the engine is designed to run on most linux desktops, relying on ALSA for sound, X11 as a means to access graphics hardware. Currently, the engine is entirely software based and cannot use any hardware acceleration.

## Getting Started

Download all source files and directories and type make on the command line. The makefile here supposes GNU/Linux. The port of the platform layer to Windows will take awhile.

## Prerequisites

There are no prerequisites other than X and ALSA. These are usually already installed on most linux desktops. 

## Installing

Here's what you can do to get it running:
1. Download all files and folders here into a folder on your computer
2. Navigate to that folder on the command line (open a terminal window, type cd [folder])
3. Type 'make dev' for an optimized version of the engine. Typing 'make' alone produces a debug version that will give you more info about application behavior.
4. Type 'make install.' If that doesn't work, copy the binary produced (found in the src/GUI/ folder) into your app path (e.g., usr/local/bin). You could also not do that and just run it wherever you copy the binary file.
5. Run paqij by typing ./linux_paqij in the folder and it will give you specifics on command line options.

I didn't write any unit tests yet. Usually not useful for a video game anyway, though I may use it for the questions put into the actual game.

## Deployment

See the notes above for installation. Deployment is no different than any other CLI application.

### Built With
C++, though I don't use most features of C++. Operator overloading and function overloading so far.

## Contributing

Let me know if you have a pull request or something else: jake.kartchner@gmail.com

## Versioning

I'm not too concerned with versioning.
Authors

    Jake Kartchner

License

This project is licensed under a modified MIT License - see the LICENSE.md file for details
Acknowledgments

    Hat tip to Casey Muratori of handmade hero fame for detailed education on game engines.
    Inspiration - I have 3 kids and want to make something they can use to help them to learn to read or other topics in school.

