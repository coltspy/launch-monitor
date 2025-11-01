# DIY Golf Launch Monitor

A camera-based golf launch monitor using dual USB cameras and OpenCV for ball tracking.

## What it does

Tracks golf ball speed, launch angle, and estimated carry distance using two vertically-stacked cameras with IR illumination. When a ball passes through the detection zone, the system captures burst frames and calculates metrics based on timing and position changes.

## Hardware

- 2x Innomaker USB cameras (1280x720 @ 120fps, global shutter, monochrome)
- IR light for ball illumination
- Cameras mounted 5 inches apart vertically in a case

## Build & Run

```bash
cd launch-monitor/build
make -j4
./launch_monitor
```

## Controls

- **View menu**: Toggle overlay, detection visualization, flip cameras
- **Start/Stop**: Begin/end monitoring mode
- **Swap Cameras**: Fix camera order if they enumerate wrong
- **Reset**: Clear shot data

## What works

- Dual camera capture at 120fps
- Motion detection triggers burst capture
- Ball detection using brightness threshold + circularity filtering
- Speed calculation from inter-camera timing
- Launch angle estimation from position tracking
- Live visualization of detected balls
- 3 frame thumbnails from each shot

## Issues & TODO

**Current problems:**
- Cameras have narrow FOV - ball needs to be pretty close to the monitor
- IR lighting needs to be stronger/better positioned for consistent detection
- Detection threshold (200 brightness) may need tuning based on lighting conditions
- Frame rate caps out at 120fps which is decent but higher would be better for accuracy

**Need to do:**
- Calibration system for camera positioning/alignment
- Better IR lighting setup (current setup is inconsistent)
- Spin detection (would need to see ball dimples/markings)
- Save/load camera settings so you don't have to flip/swap every time
- Digital zoom for better ball visibility
- Tune detection parameters (area, circularity thresholds)
- Better distance calculation (current formula is simplified)

## How it works

System uses stereo vision with vertically separated cameras. Both cameras see the ball simultaneously from different angles. By tracking the ball's position in both views over time and using the known 5" separation, we can calculate 3D position and velocity.

Detection pipeline:
1. Continuous monitoring watches for motion
2. Motion triggers 15-frame burst capture
3. Each frame gets thresholded (>200 brightness = ball)
4. Contour detection finds circular shapes
5. Best circular blob is tracked as the ball
6. Timing between cameras gives speed
7. Position changes give launch angle

The 120fps is actually pretty good but commercial systems run 200-500fps. Higher frame rates mean more data points and better accuracy, especially for fast shots.
