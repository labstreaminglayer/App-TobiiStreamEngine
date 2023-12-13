# Application Description

This is not intended for research use. Please check the [Software License Agreement](https://developer.tobii.com/license-agreement/) to see if your use case is compliant.

This app streams data from Tobii consumer devices over LSL.

It provides 2 streams:

* 2-D gaze points on screen in normalized coordinates is provided.
* Tobii events

Note that on my system the 2-D gaze points are relatively reliable at 30 Hz.
However, I opted to go with 'IRREGULAR_RATE' because I don't know if this rate is consistent across systems, and I couldn't figure out a way to inspect the rate with the SDK.

# Download

First, determine if you are compliant with the SLA (NOT FOR RESEARCH).
Find out [here](https://developer.tobii.com/license-agreement/).

If your use case is compliant then you can find the latest release of this app on [the release page](https://github.com/labstreaminglayer/App-TobiiStreamEngine/releases).
Note that you will need to provide your own copy of tobii_stream_engine.dll from the SDK and put it in the extracted folder.

# Build

## Build Dependencies

Check the SLA and if your use case is compliant then download and unzip the Stream Engine for Windows x64 from the bottom of
[this page](https://developer.tobii.com/consumer-eye-trackers/stream-engine/getting-started/). The unzipped folder name will be provided to cmake as `Tobii_ROOT`. (see below)

Download and unzip the latest liblsl binaries for Win64 from [the liblsl release page](https://github.com/sccn/liblsl/releases).
The unzipped folder will be provided to cmake as `LSL_INSTALL_ROOT`.

## Config

Follow the generic LSL-App build instructions for building apps using
Visual Studio's integrated CMake [here](https://labstreaminglayer.readthedocs.io/dev/app_build.html).

Here is an example CMakeSettings.json:

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

