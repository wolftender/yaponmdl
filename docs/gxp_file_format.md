# The Patapon GXP format
The GXP file format is mostly based on PSP GE (graphics engine) commands. 

## File format outline
You can check out the ``hexpat`` file for [ImHex](https://github.com/werwolv/imhex) editor that allows one to easily inspect any GXT or GXX file. The file is available in [gxp_patapon.hexpat](gxp_patapon.hexpat) and should be mostly self-explanatory.

## Flavors
Patapon GXP format comes in multiple flavors:
  - The GXT flavor contains mostly textures. 
  - The GXX flavor contains mostly geometry and drawlists data. Can contain frames of animations, but the animations are not interpolated (such as in GMO file format). This will be described later in the document.

## PSP GE Basics
The GE is a fixed-pipeline graphics processor. While the processor itself is fixed-pipeline (this means it is not programmable using shaders, such as more modern units like the one present in PSVita), it does accept buffers that may encoded command lists.

The command lists are simply buffers of ``uint32_t``-sized commands. Each command contains a 8-bit opcode and a 24-bit parameter. So a full program buffer can look something like this:
```
01 00 00 C2 - opcode C2 arg 01 00 00 
04 00 00 C3 - opcode C3 arg 04 00 00
00 02 00 A8 - opcode A8 arg 00 02 00
```

When reading the command buffers do mind the endianness. The opcode resides in 8 most significant bits, while the parameter resides in 24 least significant bits of each 32-bit word. Example command parser will look like this:

```c
void parse_commands(const uint32_t *buffer, uint32_t num_commands) {
    for (uint32_t pc = 0; pc < num_commands; ++pc) {
        const uint8_t opcode = (uint8_t)((*buffer >> 24) & 0xff);
        const uint32_t arg = buffer & 0x00ffffff;

        // handle command here

        buffer++;
    }
}
```

This is the core priniciple that the GXP file format operates on. The file will contain encoded command buffers that can be immediately mapped to memory accessible by GE. Furthermore, command lists can contain ``jump`` instructions that will perform jumps to other command lists, as well as ``ret`` instructions that will return the commandlist flow to the address that executed a jump. 

This is very convenient because the GXP format is hierarchical. This means that it can contain "children". This is, for example, how GXT and GXX links are implemented.

## GXP motions
Most valid GXP objects will contain "motions" (it is not clear if this is the official name, but this is how such constructions are called in IL2cpp assemblies in the remake). This is true for both the GXX and GXT variant. 

In the case of GXT, most textures will just contain a single motion with a single frame. This frame contains the GE setup drawlist for the texture. It will have a (local) pointer to the raw texture bytes as well as GE instruction that setup the parameters and then supply the texture buffer. 

In the case of GXX, the files will often contain multiple motions with multiple frames. Each motion contains a framerate parameter (a single ``float32``) that defines the "timestep" between two frames. You can calculate the delta time as ``1.0f / framerate``. Basically, each frame is a drawlist of its own, it can contain arbitrary instructions and jump to arbitrary other commandlists. What seems to be common though, is that all "frames" and with a ``ret``. This return signifies that the drawlist for the frame has been finished.

The limitation of the format is that there is no support for interpolation between motion frames. You can check on your own the file ``chr68_02_01_1.gxx`` which contains weighted matrix animations. The file has a fixed framerate of 20 FPS, and each frame will separately set all of the necessary bone matrices using ``0x2A`` and ``0x2B`` opcodes.

## Named nodes
GXP files can contain named nodes. This feature is used for GXX files (for GXT files it wouldn't make much sense). Named nodes allow, for example, to have named mount points for weapons and helmets for Zigoton/Karmen models. The named node strucutre contains two fields, the name offset, which simply points to a null-terminated string in the file and a ``uint32_t`` offset parameter. This offset parameter allows fetching the node transform for each animation frame.

Suppose you are rendering N-th animation frame from the GXX file. You can obtain transform matrix for named node in this frame if you take the start pointer of the frame (the address from which you started interpreting the GE instructions) and add the node offset to this address. You should land on a ``0x3B`` opcode. There should be 12 such opcodes at this address, giving you the full affine transform of the node. 

Do note that there will not be a ``0x3A`` code, and often this matrix is encoded after the final ``ret`` of the frame, so normally it could be considered "dead code".
