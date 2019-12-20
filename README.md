# Application Description

Stream data from Tobii consumer devices over LSL.
Has 2 streams: 

* 2-D gaze points on screen in normalized coordinates is provided.
* Tobii events

Note that on my system the 2-D gaze points are relatively reliable at 30 Hz.
However, I opted to go with 'IRREGULAR_RATE' because I don't know if this rate
is consistent across systems, and I couldn't figure out a way to inspect the rate
with the SDK.

## Download

Find the latest release on [the release page](https://github.com/labstreaminglayer/App-TobiiStreamEngine/releases).
Note that you will need to provide your own copy of tobii_stream_engine.dll and put it in the same folder.
Find it [here](https://developer.tobii.com/consumer-eye-trackers/stream-engine/getting-started/)

## Build Dependencies

Download and unzip the Stream Engine for Windows x64 from the bottom of
[this page](https://developer.tobii.com/consumer-eye-trackers/stream-engine/getting-started/).
I downloaded stream_engine_windows_x64_4.1.0.3.zip and unzipped it into C:\Users\chboulay\Tools\stream_engine_windows_x64
This folder name will be provided to cmake as `Tobii_ROOT`.

Download and unzip the latest liblsl binaries for Win64 from [the liblsl release page](https://github.com/sccn/liblsl/releases).
I downloaded liblsl-1.13.0-Win64.7z. I unzipped into C:\Users\chboulay\Tools\liblsl\liblsl .
This folder name will be provided to cmake as `LSL_INSTALL_ROOT`.

# Build

Follow the generic LSL-App build instructions for building apps using
Visual Studio's integrated CMake [here](https://labstreaminglayer.readthedocs.io/dev/app_build.html).

Here is what I had to add to my CMakeSettings.json:

```
,
      "variables": [
        {
          "name": "Qt5_DIR",
          "value": "C:\\Qt\\5.12.6\\msvc2017_64\\lib\\cmake\\Qt5 "
        },
        {
          "name": "Tobii_ROOT",
          "value": "C:\Users\chboulay\Tools\stream_engine_windows_x64"
        },
        {
          "name": "LSL_INSTALL_ROOT",
          "value": "C:\Users\chboulay\Tools\liblsl"
        }
		]
```

# License

Please see the included LICENSE.txt

