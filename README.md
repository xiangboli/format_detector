# Video Format Detector

This tool can be used to check if a given video is progrossive, interlace or telecine. 

Installation:
```bash
make clean
make
```
Usage: `format_detector [-i] input [-options]`

```
Options:

  -i, --input       <string>             Name of uncompressed video file to be analyzed (.yuv or .y4m)
  -r, --resolution  <int x int>          Video resolution (width x height, in pixels)
  -f, --framerate   <float or fraction>  Framerate (in fps)
  -c, --csp         <string>             Chroma sub-sampling format (e.g. "yuv420p", "yuv422p", etc)
  -y  --temp_dir <directory>             Directory to use for intermediate files
  -v, --verbose                          Print internal statistics & debug information
  -h, --help                             Display help
```
