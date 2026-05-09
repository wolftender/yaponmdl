# The GMO File format
The GMO file format is a common PSP model format used by many games. It is often used most likely because the official SDK contained a PSP library for parsing and rendering GMO models as well as a toolchain that allows converting FBX (common scene transfer format in the industry, developed by Autodesk) to GMO models. 

If you check the bone names in Patapon GMO models, it seems that this is exactly what the developers of the game did, as most models will contain a root bone named ``MAXSceneRoot``.

## File structure
Each GMO file is made up by:
 - A 16-byte GMO header, containing four 4-byte fields ``signature``, ``version``, ``style`` and unused 4th value that is always zero
 - A tree-like structure of "chunks" and "half-chunks". Chunks are mostly used for higher-level objects such as a model or a motion, they can also be named. Half-chunks are used mostly for unnamed child objects such as properties of materials or bone parameters.

The GMO header should contain the following values for a PSP model:
| type | field | value |
|------|-------|-------|
| ``uint32_t`` | ``signature`` | ``0x2e474d4f`` |
| ``uint32_t`` | ``version``   | ``0x312e3030`` |
| ``uint32_t`` | ``style``     | ``0x00505350`` |
| ``uint32_t`` | ``option``    | ``0x00000000`` |

After the header, the file will contain chunks and half-chunks. All chunks will always contain the fields:
| type | field | description |
|------|-------|-------|
| ``uint16_t`` | ``type``       | enumerated chunk type, if the MSB is set, then this is a half chunk |
| ``uint16_t`` | ``args_offs``  | chunk arguments offset, used only for full chunks, for half chunks simply add ``8`` to the chunk pointer |
| ``uint32_t`` | ``next_offs``  | next same-level chunk offset, next chunk address is calculated as current chunk addres plus next offset |

A full chunk will additionally contain fields
| type | field | description |
|------|-------|-------|
| ``uint32_t`` | ``child_offs`` | pointer to the first child is calculated as chunk pointer plus this value |
| ``uint32_t`` | ``data_offs`` | not used |

Most common operation is traversing the chunk tree. To find all children of a chunk, you can use the ``child_offs`` and ``next_offs``:
```c
void traverse_children(const Chunk *chunk) {
    Chunk *child = chunk->child_offs;
    const Chunk *next = chunk->next_offs;

    while (child < next) {
        // do something with the child chunk here

        traverse_children(child); // traverse children of the child chunk recursviely
        child = child->next_offs;
    }
}
```

## Patapon-specific properties
Patapon uses some custom properties (mostly in animations).
- Property type ``0x4100``, is used as an animation target property of a node. An animation targetting a bone will target the ``0x4100`` property which is "bone UV offset". The FCurve keyframes will have values for a 2-dimensional float vector. This property sets all drawn meshes UV offset to be ``vertex.uv + uv_offset``.
- Property type ``0x4101``, is used as an animation target property of a node. This property is a single floating-point value (for each frame) that changes the nodes alpha. The final color should additionally have alpha multiplied by this value. This is used to hide parts of bosses in animations or during death animations for hunting animals.
- Target type ``0x7fff``, unknown purpose. Some animation channels will target an object of this type. This is not native to the GMO file format. Modified properties are translation, rotation and scale.
