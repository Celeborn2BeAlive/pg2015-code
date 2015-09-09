# pg2015-code

This repository contains the source code for our method proposed at the conference [Pacific Graphics 2015](http://cg.cs.tsinghua.edu.cn/pg2015/).

The purpose of this method is to improve the Bidirectional Path Tracing (BPT) algorithm by resampling connections between path vertices, taking advantage of the information offered by a skeleton of the empty space of the scene. For more information, you can refer to [our paper](http://www.laurentnoel.fr/research/papers/Noel2015-PG.pdf). The paper is also provided in the repository.

### Compilation

The project compilation is managed with CMake. All dependencies are normaly included in the "third-party" folder (headers and 64 bit binaries). The project has been tested with the following configurations:

* Linux 64 bit with GCC 5.2
* Windows 10 64 bit with Visual C++ 2015 Community Edition

Generate one of the two above CMake solution depending on your system and compile the project either with make or with Visual C++ IDE.

Note that the library "bonez" in the repository depends on recent OpenGL 4.1+ features, thus we cannot guarentee that the code will work with older versions of OpenGL (even if the application "pg2015" does not use such features).

### Usage

The project generates two executables: pg2015 and results_viewer.

The application pg2015 launches the rendering and comparison of the three methods mentionned in the paper: BPT, ICBPT and SkelBPT.
The scenes presented in the article have been put in the repository [pg2015-scenes](https://github.com/Celeborn2BeAlive/pg2015-scenes), so clone this repository and launch "pg2015" from it.
Note that file paths are hard-coded in the main.cpp file, so the working directory of the application should be the directory "pg2015-scenes" (or the file paths should be changed in the source code).

The application results_viewer is just a viewer for the rendered images in EXR format (the application also output PNG files so result_viewer is not strictly required).

### Troubles

If you face any troubles during the compilation or the execution, please leave an issue on the repository web page on Github.