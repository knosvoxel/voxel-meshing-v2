# voxel-meshing-v2
Implementation of two voxel greedy meshing algorithms.

Created for my Master's thesis with the title "*Tradeoffs between different voxel rendering techniques for static scenes*" at Hochschule der Medien, Stuttgart

## Branches
*main*: A GPU-based slicing approach
*binary-greedy-meshing*: An implementation of binary greedy meshing based on cgerikj's [binary-greedy-meshing](https://github.com/cgerikj/binary-greedy-meshing) with support for loading MagicaVoxel's .vox file format
*gpu-with-quad-reconstruction*: Similar to the slicing approach but with a memory-efficient quad reconstruction technique in the vertex shader that currently kills per-frame performance
*slicing-cpu*: A CPU-based implementation of the slicing approach using multi-threading
