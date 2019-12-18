# Application Description

Stream data from Tobii consumer devices over LSL.

## Dependencies

Download and unzip the Stream Engine for Windows x64 from the bottom of
[this page](https://developer.tobii.com/consumer-eye-trackers/stream-engine/getting-started/).
I downloaded stream_engine_windows_x64_4.1.0.3.zip and unzipped it into D:\Tools\Misc\stream_engine_windows_x64

Download and unzip the latest liblsl binaries for Win64 from [the liblsl release page](https://github.com/sccn/liblsl/releases).
I downloaded liblsl-1.13.0-Win64.7z. I unzipped into D:\Tools\Misc\liblsl_install

## Download

When the app is done, downloads will be available on the releases page.

# Build

Follow the generic LSL-App build instructions for building apps using
Visual Studio's integrated CMake. See [here](https://labstreaminglayer.readthedocs.io/dev/build.html#configure-cmake-options-in-vs-2017-vs-2019).
Note the docs are a bit confusing right now. This should be updated soon.

Here is what I had to add to my CMakeSettings.json:

```
,
      "variables": [
        {
          "name": "Qt5_DIR",
          "value": "C:\\Qt\\5.11.1\\msvc2017_64\\lib\\cmake\\Qt5 "
        },
        {
          "name": "Tobii_ROOT",
          "value": "D:\\Tools\\Misc\\stream_engine_windows_x64"
        },
        {
          "name": "LSL_INSTALL_ROOT",
          "value": "D:\\Tools\\Misc\\liblsl_install"
        }
```

# License

Please see the included LICENSE.txt

