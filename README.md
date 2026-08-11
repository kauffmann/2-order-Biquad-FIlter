A Biquad 2-order IIR filter. LPF, HPF, Notch, BandPass, HighShelf and LowShelf. 

![FIlter](https://github.com/user-attachments/assets/a6da1ee7-454e-4566-9613-9e2a6e88fb37)






Creates an audio plugin (e.g., a VST3 plugin). Created with JUCE 8, C++ and CMake. 



## Usage

After cloning it locally, you can proceed with the usual CMake workflow.

In the main repo directory execute

```bash
$ cmake -S . -B build
$ cmake --build build
```

The first run will take the most time because the dependencies (CPM, JUCE, and googletest) need to be downloaded.

On Mac/Xcode you must first run config from terminal, creating a .xcodeproj file you can open in xcode(cmake -S . -B build -G Xcode).
In visual studio and visual studio code you can do this within editor IDE, using build in terminal.







