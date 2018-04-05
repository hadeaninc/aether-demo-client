# Hadean Aether Client Examples

## What is Aether Engine?

Aether Engine is a Distributed Simulation Engine designed to effortlessly
create virtual worlds of unprecedented scale and complexity.

## This repository

The aim is to give some examples of what it feels like to write a client
for Aether.

Here you'll find two clients: a simple OpenGL client, and a client
built using the [Godot] engine.  In addition, the file
`aether_recording.dump` contains some sample data produced by a
running Aether instance.

Here's a screenshot of the result in OpenGL:

![Screenshot of flocking particles in OpenGL](/screenshot.png)

## Building

You can build the demos:

### Ubuntu

For Ubuntu 17.10:
``` shellsession
$ sudo apt install build-essential libglfw3 libglfw3-dev libglew2.0 libglew-dev
```
Or for Ubuntu 16.04 LTS:
``` shellsession
$ sudo apt install build-essential libglfw3 libglfw3-dev libglew1.13 libglew-dev
```

If you want to try the Godot client you will also need [Godot] and the
[GDNative headers] installed and in your include path.  After ensuring
the dependencies are installed, simply
``` shellsession
$ make
```

To build only the OpenGL client, instead
``` shellsession
$ make opengl
```

Or to build only the Godot client,
``` shellsession
$ make godot
```

### With Nix

You'll need a recent checkout of nixpkgs (`nixos-unstable` or more
recent).  Then, just
``` shellsession
$ nix-build -E 'with import <nixpkgs> { }; callPackage ./. { }'
```
to get all dependencies and build all the clients.

### With Other Package Managers

We don't do anything weird: so long as you can install GLEW, GLFW3,
and Godot, your distribution will probably work (but we haven't tested
it).  Make sure the dependencies are in place, then just run `make`.

## Running

### OpenGL

Run the OpenGL client with the path to the dump.
``` shellsession
./clients/opengl/bin/client aether_recording.dump
```

### Godot

To run the Godot client, you'll need to install the [Godot] engine for
your distribution.  Then, after building, simply run
`GODOT_REPLAY_FILE=/path/to/aether_recording.dump godot --path
clients/godot`.  Beware that this path is interpreted relative to the
Godot client (containing `project.godot`).

## Licensing

All code in this repository is licensed under the Apache 2.0 licence,
which can be found in the [LICENSE](LICENSE) file next to this one.

The images and sample data are licensed under the Creative Commons
[Attribution-NonCommercial-NoDerivatives 4.0 International licence](cc-nc-nd).

[Godot]: https://godotengine.org/
[GDNative headers]: https://github.com/GodotNativeTools/godot_headers
[cc-nc-nd]: https://creativecommons.org/licenses/by-nc-nd/4.0/
