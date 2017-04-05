#include "world_map.h"

#include <fstream>

namespace proslam {
  using namespace srrg_core;

  WorldMap::WorldMap() {
    std::cerr << "WorldMap::WorldMap|constructing" << std::endl;
    clear();
    std::cerr << "WorldMap::WorldMap|constructed" << std::endl;
  };

  WorldMap::~WorldMap() {
    std::cerr << "WorldMap::WorldMap|destroying" << std::endl;

    //ds free landmarks
    for(LandmarkPointerMap::iterator it = _landmarks.begin(); it != _landmarks.end(); ++it) {
      delete it->second;
    }

    //ds free landmarks in window
    for(LandmarkPointerMap::iterator it = _landmarks_in_window_for_local_map.begin(); it != _landmarks_in_window_for_local_map.end(); ++it) {
      delete it->second;
    }

    //ds free all frames (also destroys local maps by their anchors)
    for(FramePointerMap::iterator it = _frames.begin(); it != _frames.end(); ++it) {
      delete it->second;
    }

    //ds clear all stacks
    clear();
    std::cerr << "WorldMap::WorldMap|destroyed" << std::endl;
  }
  
  //ds clears all internal structures
  void WorldMap::clear() {
    _frame_queue_for_local_map.clear();
    _landmarks_in_window_for_local_map.clear();
    _landmarks.clear();
    _frames.clear();
    _local_maps.clear();
    _currently_tracked_landmarks.clear();
  }

  //ds creates a new frame living in this instance at the provided pose
  Frame* WorldMap::createFrame(const TransformMatrix3D& robot_to_world_, const real& maximum_depth_close_){
    if (_previous_frame) {
      _previous_frame->releaseImages();
    }
    _previous_frame = _current_frame;
    _current_frame  = new Frame(this, _previous_frame, 0, robot_to_world_, maximum_depth_close_);
    if (_root_frame == 0) {
      _root_frame = _current_frame;
    }
    if (_previous_frame) {
      _previous_frame->setNext(_current_frame);
    }

    //ds bookkeeping
    _frames.put(_current_frame);
    _frame_queue_for_local_map.push_back(_current_frame);
    return _current_frame;
  }

  //ds creates a new landmark living in this instance, using the provided framepoint as origin
  Landmark* WorldMap::createLandmark(const FramePoint* origin_){
    Landmark* landmark = new Landmark(origin_);
    _landmarks_in_window_for_local_map.put(landmark);
    return landmark;
  }

  //ds attempts to create a new local map if the generation criteria are met (returns true if a local map was generated)
  const bool WorldMap::createLocalMap() {
    if (_previous_frame == 0) {
      return false;
    }

    //ds reset closure status
    _relocalized = false;

    //ds update distance traveled and last pose
    const TransformMatrix3D robot_pose_last_to_current = _previous_frame->worldToRobot()*_current_frame->robotToWorld();
    _distance_traveled_window += robot_pose_last_to_current.translation().norm();
    _degrees_rotated_window   += toOrientationRodrigues(robot_pose_last_to_current.linear()).norm();

    //ds check if we can generate a keyframe - if generated by translation only a minimum number of frames in the buffer is required - or a new tracking context
    if (_degrees_rotated_window   > _minimum_degrees_rotated_for_local_map                                                                                 ||
        (_distance_traveled_window > _minimum_distance_traveled_for_local_map && _frame_queue_for_local_map.size() > _minimum_number_of_frames_for_local_map)||
        (_frame_queue_for_local_map.size() > _minimum_number_of_frames_for_local_map && _local_maps.size() < 5)                                              ) {

      //ds create the new keyframe and add it to the keyframe database
      _current_local_map = new LocalMap(_frame_queue_for_local_map);
      _local_maps.push_back(_current_local_map);

      //ds reset generation properties
      resetWindowForLocalMapCreation();

      //ds current frame is now center of a local map - update structures
      _current_frame = _current_local_map;
      _frames.replace(_current_frame);

      //ds local map generated
      return true;
    } else {

      //ds no local map generated
      return false;
    }
  }

  //ds resets the window for the local map generation
  void WorldMap::resetWindowForLocalMapCreation() {
    _distance_traveled_window = 0;
    _degrees_rotated_window   = 0;

    //ds free memory if desired (saves a lot of memory costs a little computation)
    if (_drop_framepoints) {

      //ds the last frame well need for the next tracking step
      _frame_queue_for_local_map.pop_back();

      //ds purge the rest
      for (Frame* frame: _frame_queue_for_local_map) {
        frame->releasePoints();
      }
    }
    _frame_queue_for_local_map.clear();

    //ds temporary buffer
    LandmarkPointerMap landmarks_in_window_tracked;
    landmarks_in_window_tracked.clear();

    //ds loop over the landmarks in the current window - either delete them, keep them or add them to the final landmarks map if theyre part of a local map
    for(LandmarkPointerMap::iterator it = _landmarks_in_window_for_local_map.begin(); it != _landmarks_in_window_for_local_map.end(); ++it) {

      //ds if the landmark is not part of a local map
      if (it->second->localMap() == 0) {

        //ds if the landmark is currently tracked - it might still become part of a local map
        if (it->second->isCurrentlyTracked()) {

          //ds we want to keep it
          landmarks_in_window_tracked.put(it->second);
        } else {

          //ds free the landmark
          delete it->second;
        }
      } else {

        //ds add the landmark permanently
        _landmarks.put(it->second);
      }
    }

    //ds update current window
    _landmarks_in_window_for_local_map.clear();
    _landmarks_in_window_for_local_map.swap(landmarks_in_window_tracked);
  }

  //ds adds a loop closure constraint between 2 local maps
  void WorldMap::addLoopClosure(LocalMap* query_,
                                const LocalMap* reference_,
                                const TransformMatrix3D& transform_query_to_reference_,
                                const real& omega_) {
    query_->add(reference_, transform_query_to_reference_, omega_);
    _relocalized = true;

    //ds informative only
    ++_number_of_closures;
  }

  //ds dump trajectory to file (in KITTI benchmark format: 4x4 isometries per line)
  void WorldMap::writeTrajectory(const std::string& filename_) const {

    //ds construct filename
    std::string filename(filename_);

    //ds if not set
    if (filename_ == "") {

      //ds generate generic filename with timestamp
      filename = "trajectory-"+std::to_string(static_cast<uint64_t>(std::round(srrg_core::getTime())))+".txt";
    }

    //ds open file stream (overwriting)
    std::ofstream outfile_trajectory(filename, std::ifstream::out);
    assert(outfile_trajectory.good());

    //ds for each frame (assuming continuous, sequential indexing)
    for (Index index_frame = 0; index_frame < _frames.size(); ++index_frame) {

      //ds buffer transform
      const TransformMatrix3D& robot_to_world = _frames.at(index_frame)->robotToWorld();

      //ds dump transform according to KITTI format
      for (uint8_t u = 0; u < 3; ++u) {
        for (uint8_t v = 0; v < 4; ++v) {
          outfile_trajectory << robot_to_world(u,v) << " ";
        }
      }
      outfile_trajectory << "\n";
    }
    outfile_trajectory.close();
    std::cerr << "WorldMap::WorldMap|saved trajectory to: " << filename << std::endl;
  }
}

