# ESP-GMF-Misc

- [中文版](./README_CN.md)

ESP GMF Miscellaneous gathers various GMF elements with single-purpose functionalities. The currently supported elements are listed below：
|  Name  | Function Description | Input Port | Output port |Input blocking time|Output blocking time|
|:----:|:----:|:----:|:----:|:----:|:----:|
| esp_gmf_copier | Copies input data to multiple output ports | Single | Multiple | Maximum delay | User configurable, default value is maximum delay |

## Usage
ESP GMF Miscellaneous is often combined with other GMF elements to form a pipeline. For example code, please refer to [test_app](../test_apps/main/elements/gmf_audio_play_el_test.c)。
