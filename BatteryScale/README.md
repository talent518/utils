# Android充电模式动画
android 5.1.1 中充电模式图片(动画)的以及由PHP实现的工具，Android系统源码目录为 system/core/healthd/images

## 工具说明
  * split_battery_scale.php => 把battery_scale.png转换成每帧一个图片(battery_scale_<n>.png)
  * make_battery_scale.php => 是split_battery_scale.php相反操作

## 图片
  * battery_scale.png => 充电动画图片(多帧方式)
  * battery_fail.png => 充电异常图片

## 问题
因读取不到Frames参数而报"battery_scale image has unexpected frame count (1, expected 6)"错问题，解决方式暂时只能改为一个图片一帧的方式加载动画帧数据。
