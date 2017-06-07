#pragma once
#include "landmark.h"

namespace proslam {

  //ds this class condenses a group of Frame objects into a single Local Map object, which used for relocalization and pose optimization
  class LocalMap {

  //ds exported types
  public: EIGEN_MAKE_ALIGNED_OPERATOR_NEW

      //ds loop closure constraint element between 2 local maps
      struct Closure {
      EIGEN_MAKE_ALIGNED_OPERATOR_NEW
        Closure(const LocalMap* local_map_,
                const TransformMatrix3D& relation_,
                const real& omega_ = 1): local_map(local_map_),
                                         relation(relation_),
                                         omega(omega_){}

        const LocalMap* local_map;
        const TransformMatrix3D relation;
        const real omega;
      };
      typedef std::vector<Closure> ClosureVector;

  //ds object handling
  protected:

    //! @brief constructs a local map that lives in the reference frame of the consumed frames
    //! @param[in] frames_ the collection of frames to be contained in the local map (same track)
    //! @param[in] local_map_root_ the first local map in the same track
    //! @param[in] local_map_previous_ the preceding local map in the same track
    //! @param[in] minimum_number_of_landmarks_ target minimum number of landmarks to contain in local map
    LocalMap(FramePointerVector& frames_, LocalMap* local_map_root_ = 0, LocalMap* local_map_previous_ = 0, const Count& minimum_number_of_landmarks_ = 50);

    //ds cleanup of dynamic structures
    ~LocalMap();

    //ds prohibit default construction
    LocalMap() = delete;

  //ds functionality:
  public:

    //! @brief clears all internal structures (prepares a fresh world map)
    void clear();

    //! @brief updates local map pose, automatically updating contained Frame poses (pyramid)
    //! @param[in] local_map_to_world_ the local map pose with respect to the world map coordinate frame
    void update(const TransformMatrix3D& local_map_to_world_);

    //! @brief adds a loop closure constraint between this local map and a reference map
    //! @param[in] local_map_reference_ the corresponding reference local map
    //! @param[in] transform_query_to_reference_ the spatial relation between query and reference (from query to reference)
    //! @param[in] omega_ 1D information value of the correspondence
    void addCorrespondence(const LocalMap* local_map_reference_,
                           const TransformMatrix3D& query_to_reference_,
                           const real& omega_ = 1) {_closures.push_back(Closure(local_map_reference_, query_to_reference_, omega_));}

  //ds getters/setters
  public:

    inline const Identifier& identifier() const {return _identifier;}

    inline const TransformMatrix3D& localMapToWorld() const {return _local_map_to_world;}
    inline const TransformMatrix3D& worldToLocalMap() const {return _world_to_local_map;}
    void setLocalMapToWorld(const TransformMatrix3D& local_map_to_world_) {_local_map_to_world = local_map_to_world_; _world_to_local_map = _local_map_to_world.inverse();}
    void setWorldToLocalMap(const TransformMatrix3D& world_to_local_map_) {_world_to_local_map = world_to_local_map_; _local_map_to_world = _world_to_local_map.inverse();}

    inline LocalMap* root() {return _root;}
    void setRoot(LocalMap* root_) {_root = root_;}
    inline LocalMap* previous() {return _previous;}
    void setPrevious(LocalMap* local_map_) {_previous = local_map_;}
    inline LocalMap* next() {return _next;}
    void setNext(LocalMap* local_map_) {_next = local_map_;}
    inline const Frame* keyframe() const {return _keyframe;}

    inline const HBSTNode::MatchableVector& appearances() const {return _appearances;}
    inline const Landmark::StatePointerVector& landmarks() const {return _landmarks;}

    inline const ClosureVector& closures() const {return _closures;}

  //ds attributes
  protected:

    //ds unique identifier for a local map (exists once in memory)
    const Identifier _identifier;

    //! @brief pose of the local map with respect to the world map coordinate frame
    TransformMatrix3D _local_map_to_world;

    //! @brief transform to map world geometries into the local map coordinate frame
    TransformMatrix3D _world_to_local_map;

    //ds links to preceding and subsequent instances
    LocalMap* _root;
    LocalMap* _previous;
    LocalMap* _next;

    //! @brief the keyframe of the local map
    Frame* _keyframe;

    //ds the contained Frames
    FramePointerVector _frames;

    //ds landmarks in the configuration at the time of the creation of the local map
    Landmark::StatePointerVector _landmarks;

    //ds one merged pool of all corresponding landmark appearances
    HBSTNode::MatchableVector _appearances;

    //ds loop closures for the local map
    ClosureVector _closures;

    //ds grant access to local map producer
    friend WorldMap;

  //ds class specific
  private:

    //! @brief inner instance count - incremented upon constructor call (also unsuccessful calls)
    static Count _instances;
  };

  typedef std::vector<LocalMap*> LocalMapPointerVector;
}
