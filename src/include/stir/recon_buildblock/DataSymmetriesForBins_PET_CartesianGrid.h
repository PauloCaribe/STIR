//
// $Id$
//
/*!

  \file
  \ingroup symmetries

  \brief Declaration of class DataSymmetriesForBins_PET_CartesianGrid

  \author Kris Thielemans
  \author PARAPET project

  $Date$
  $Revision$
*/
/*
    Copyright (C) 2000 PARAPET partners
    Copyright (C) 2000- $Date$, Hammersmith Imanet Ltd
    See STIR/LICENSE.txt for details
*/
#ifndef __DataSymmetriesForBins_PET_CartesianGrid_H__
#define __DataSymmetriesForBins_PET_CartesianGrid_H__


#include "stir/recon_buildblock/DataSymmetriesForBins.h"
//#include "stir/SymmetryOperations_PET_CartesianGrid.h"
//#include "stir/ViewSegmentNumbers.h"
//#include "stir/VoxelsOnCartesianGrid.h"
#include "stir/Bin.h"

START_NAMESPACE_STIR

template <int num_dimensions, typename elemT> class DiscretisedDensity;
template <int num_dimensions, typename elemT> class DiscretisedDensityOnCartesianGrid;
template <typename T> class shared_ptr;
class ProjMatrixByBinFromFile;
/*!
  \ingroup symmetries
  \brief Symmetries appropriate for a (cylindrical) PET scanner, and 
  a discretised density on a Cartesian grid.

  All operations (except the constructor) are inline as timing of
  the methods of this class is critical.
*/
class DataSymmetriesForBins_PET_CartesianGrid : public DataSymmetriesForBins
{
public:

  //! Constructor with optional selection of symmetries
  /*! For the azimuthal angle phi, the following angles are symmetry related for a square grid:
      {phi, 180-phi, 90-phi, 90+phi}.
      The boolean parameters allow to select which angles should be considered as related:<br>
      <ul>
      <li> azimuthal</li>
      <ul>
      <li> all 4:
           (\a do_symmetry_90degrees_min_phi=true)</li>
      <li> only {phi, 180-phi} :
           (\a do_symmetry_90degrees_min_phi=false, 
            \a do_symmetry_180degrees_min_phi = true)</li>
      <li> none:
            (\a do_symmetry_90degrees_min_phi=false, 
            \a do_symmetry_180degrees_min_phi = false)</li>
      </ul>
      <li>axial (i.e. positive vs. negative segment): \a do_symmetry_swap_segment </li>
      <li>tangential (i.e. positive vs negative s): \a     do_symmetry_swap_s </li>
      <li> axial shift \a do_symmetry_shift_z </li>
      </ul>

      Note that when \a do_symmetry_90degrees_min_phi=true, the value of
      \a do_symmetry_180degrees_min_phi is irrelevant. This because otherwise a
      non-consecutive range in phi would have to be used.<br>

      The symmetry in phi is automatically reduced for non-square grids or when the number of
      views is not a multiple of 4.
  */    
  DataSymmetriesForBins_PET_CartesianGrid(const shared_ptr<ProjDataInfo>& proj_data_info_ptr,
                                          const shared_ptr<DiscretisedDensity<3,float> >& image_info_ptr,
                                          const bool do_symmetry_90degrees_min_phi = true,
                                          const bool do_symmetry_180degrees_min_phi = true,
					  const bool do_symmetry_swap_segment = true,
					  const bool do_symmetry_swap_s = true,
					  const bool do_symmetry_shift_z = true);


  virtual 
    inline 
#ifndef STIR_NO_COVARIANT_RETURN_TYPES
    DataSymmetriesForBins 
#else
    DataSymmetriesForViewSegmentNumbers
#endif
    * clone() const;
#if 0
  TODO!
  //! returns the range of the indices for basic bins
  virtual BinIndexRange
    get_basic_bin_index_range() const;
#endif

  inline void
    get_related_bins_factorised(vector<AxTangPosNumbers>&, const Bin& b,
                                const int min_axial_pos_num, const int max_axial_pos_num,
                                const int min_tangential_pos_num, const int max_tangential_pos_num) const;

  inline int
    num_related_bins(const Bin& b) const;

  inline auto_ptr<SymmetryOperation>
    find_symmetry_operation_from_basic_bin(Bin&) const;

  inline bool
    find_basic_bin(Bin& b) const;
  
  inline int
    num_related_view_segment_numbers(const ViewSegmentNumbers& vs) const;
  
  inline void
    get_related_view_segment_numbers(vector<ViewSegmentNumbers>& rel_vs, const ViewSegmentNumbers& vs) const;
  
  inline bool
    find_basic_view_segment_numbers(ViewSegmentNumbers& v_s) const;

  //! find out how many image planes there are for every scanner ring
  inline float get_num_planes_per_scanner_ring() const;



  //! find correspondence between axial_pos_num and image coordinates
  /*! <tt>z = num_planes_per_axial_pos * axial_pos_num + axial_pos_to_z_offset</tt>
  
      compute the offset by matching up the centre of the scanner 
      in the 2 coordinate systems
      */
  inline float get_num_planes_per_axial_pos(const int segment_num) const;
  inline float get_axial_pos_to_z_offset(const int segment_num) const;
  
private:
  // temporary fix to give access to the bools
  friend class ProjMatrixByBinFromFile;

  bool do_symmetry_90degrees_min_phi;
  bool do_symmetry_180degrees_min_phi;
  bool do_symmetry_swap_segment;
  bool do_symmetry_swap_s;
  bool do_symmetry_shift_z;
  //const shared_ptr<ProjDataInfo>& proj_data_info_ptr;
  int num_views;
  int num_planes_per_scanner_ring;
  //! a list of values for every segment_num
  VectorWithOffset<int> num_planes_per_axial_pos;
  //! a list of values for every segment_num
  VectorWithOffset<float> axial_pos_to_z_offset;

#if 0
  // at the moment, we don't need the following 2 members

  // TODO somehow store only the info
  shared_ptr<DiscretisedDensity<3,float> > image_info_ptr;

  // a convenience function that does the dynamic_cast from the above
  inline const DiscretisedDensityOnCartesianGrid<3,float> *
    cartesian_grid_info_ptr() const;
#endif

  inline bool
  find_basic_bin(int &segment_num, int &view_num, int &axial_pos_num, int &tangential_pos_num) const;

  
  inline int find_transform_z(
			 const int segment_num, 
			 const int  axial_pos_num) const;
  
  inline SymmetryOperation* 
    find_sym_op_general_bin(   
    int s, 
    int seg, 
    int view_num, 
    int axial_pos_num) const;
  
  inline SymmetryOperation* 
    find_sym_op_bin0(   
    int seg, 
    int view_num, 
    int axial_pos_num) const;
  
};

END_NAMESPACE_STIR

#include "stir/recon_buildblock/DataSymmetriesForBins_PET_CartesianGrid.inl"

#endif
