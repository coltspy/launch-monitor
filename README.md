<p float="left">
  <img src="menu.png" height="400" style="margin-right: 20px;" />
  <img src="imgs.png" height="400" />
</p>

## UPDATE

- currently working on strobing IR in frame instead of continuos capture - hopefully stop blurry frames
- with low fps cameras, we can sync the camera exposure to freeze the motion
- need to research led driver circuits and modules
- changing IR light, ~730nm seems to be the sweet spot

- cameras have narrow fov - research lens



Tracks golf ball speed, launch angle, and estimated carry distance using two vertically-stacked cameras with IR illumination. When a ball passes through the detection zone, the system captures burst frames and calculates metrics based on timing and position changes

## My Hardware

- 2x Innomaker camera modules (120fps, global shutter, monochrome)
- IR light for ball illumination
- Cameras mounted 5 inches apart vertically in a case - 2 angles of ball, top down & adjacent
- NVIDIA Jetson nano development kit
  
Build & Run

```bash
cd launch-monitor/build
make -j4
./launch_monitor
```

Controls

- **View menu**: Toggle overlay, detection visualization, flip cameras
- **Start/Stop**: Begin/end monitoring mode
- **Reset**: Clear shot data


