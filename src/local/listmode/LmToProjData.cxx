//
// $Id$
//
/*!
  \file 
  \ingroup utilities

  \brief Program to bin listmode data to 3d sinograms
 
  \author Kris Thielemans
  \author Sanida Mustafovic
  
  $Date$
  $Revision $
*/
/*
    Copyright (C) 2000- $Date$, IRSL
    See STIR/LICENSE.txt for details
*/

/* Possible compilation switches:
  
USE_SegmentByView 
  Currently our ProjData classes store segments as floats, which is a waste of
  memory and time for simple binning of listmode data. This should be
  remedied at some point by having member template functions to allow different
  data types in ProjData et al.
  Currently we work (somewhat tediously) around this problem by using Array classes directly.
  If you want to use the Segment classes (safer and cleaner)
  #define USE_SegmentByView


FRAME_BASED_DT_CORR:
   dead-time correction based on the frame, or on the time of the event
*/   
// (Note: can currently NOT be disabled)
#define USE_SegmentByView

#define FRAME_BASED_DT_CORR

#define INCLUDE_NORMALISATION_FACTORS

// set elem_type to what you want to use for the sinogram elements
// we need a signed type, as randoms can be subtracted. However, signed char could do.

#if defined(USE_SegmentByView) || defined(INCLUDE_NORMALISATION_FACTORS) 
   typedef float elem_type;
#  define OUTPUTNumericType NumericType::FLOAT
#else
   #error currently problem with normalisation code!
   typedef short elem_type;
#  define OUTPUTNumericType NumericType::SHORT
#endif


#include "stir/utilities.h"

#include "local/stir/listmode/LmToProjData.h"
#include "local/stir/listmode/CListRecord.h"
#include "local/stir/listmode/CListModeData.h"
#include "stir/ProjDataInfoCylindricalNoArcCorr.h"

#include "stir/Scanner.h"
#include "stir/SegmentByView.h"
#ifdef USE_SegmentByView
#include "stir/ProjDataInterfile.h"
#include "stir/SegmentByView.h"
#else
#include "stir/ProjDataFromStream.h"
#include "stir/IO/interfile.h"
#include "stir/Array.h"
#include "stir/IndexRange3D.h"
#endif
#include "stir/ParsingObject.h"
#include "stir/TimeFrameDefinitions.h"
#include "stir/CPUTimer.h"
#include "stir/recon_buildblock/TrivialBinNormalisation.h"
#include "stir/is_null_ptr.h"

#include <fstream>
#include <iostream>
#include <vector>

#ifndef STIR_NO_NAMESPACES
using std::fstream;
using std::ifstream;
using std::iostream;
using std::ofstream;
using std::cerr;
using std::cout;
using std::flush;
using std::endl;
using std::min;
using std::max;
#endif





#ifdef USE_SegmentByView
#include "stir/SegmentByView.h"
#else
#include "stir/Array.h"
#include "stir/IndexRange3D.h"
#endif

START_NAMESPACE_STIR

#ifdef USE_SegmentByView
typedef SegmentByView<elem_type> segment_type;
#else
#include "stir/Array.h"
#include "stir/IndexRange3D.h"
#endif
/******************** Prototypes  for local routines ************************/



static void 
allocate_segments(VectorWithOffset<segment_type *>& segments,
                       const int start_segment_index, 
	               const int end_segment_index,
                       const ProjDataInfo* proj_data_info_ptr);
/* last parameter only used if USE_SegmentByView
   first parameter only used when not USE_SegmentByView
 */         
static void 
save_and_delete_segments(shared_ptr<iostream>& output,
			      VectorWithOffset<segment_type *>& segments,
			      const int start_segment_index, 
			      const int end_segment_index, 
			      ProjData& proj_data);

// In the next 3 functions, the 'output' parameter needs to be passed 
// because save_and_delete_segments needs it when we're not using SegmentByView
static
shared_ptr<ProjData>
construct_proj_data(shared_ptr<iostream>& output,
                    const string& output_filename, 
                    const shared_ptr<ProjDataInfo>& proj_data_info_ptr);


void 
LmToProjData::
set_defaults()
{
  max_segment_num_to_process = -1;
  store_prompts = true;
  delayed_increment = -1;
  interactive=false;
  num_segments_in_memory = -1;
  normalisation_ptr = new TrivialBinNormalisation;
  post_normalisation_ptr = new TrivialBinNormalisation;
  do_pre_normalisation =0;
  num_events_to_store = 0;
  
}

void 
LmToProjData::
initialise_keymap()
{
  parser.add_start_key("lm_to_projdata Parameters");
  parser.add_key("input file",&input_filename);
  parser.add_key("template_projdata", &template_proj_data_name);
  parser.add_key("frame_definition file",&frame_definition_filename);
  parser.add_key("num_events_to_store",&num_events_to_store);
  parser.add_key("output filename prefix",&output_filename_prefix);
  parser.add_parsing_key("Bin Normalisation type for pre normalisation", &normalisation_ptr);
  parser.add_parsing_key("Bin Normalisation type for post normalisation", &post_normalisation_ptr);
  parser.add_key("maximum absolute segment number to process", &max_segment_num_to_process); 
  parser.add_key("do pre normalisation ", &do_pre_normalisation);
  parser.add_key("num_segments_in_memory", &num_segments_in_memory);

  //if (lm_data_ptr->has_delayeds()) TODO we haven't read the CListModeData yet, so cannot access has_delayeds() yet
  // one could add the next 2 keywords as part of a callback function for the 'input file' keyword.
  // That's a bit too much trouble for now though...
  {
    parser.add_key("Store 'prompts'",&store_prompts);
    parser.add_key("increment to use for 'delayeds'",&delayed_increment);
  }
  parser.add_key("List event coordinates",&interactive);
  parser.add_stop_key("END");  

}


bool
LmToProjData::
post_processing()
{

  if (input_filename.size()==0)
    {
      warning("You have to specify an input_filename\n");
      return true;
    }

  if (!interactive && output_filename_prefix.size()==0)
    {
      warning("You have to specify an output_filename_prefix\n");
      return true;
    }

  lm_data_ptr =
    CListModeData::read_from_file(input_filename);

  if (template_proj_data_name.size()==0)
    {
      warning("You have to specify template_projdata\n");
      return true;
    }
  shared_ptr<ProjData> template_proj_data_ptr =
    ProjData::read_from_file(template_proj_data_name);

  template_proj_data_info_ptr = 
    template_proj_data_ptr->get_proj_data_info_ptr()->clone();

  // initialise segment_num related variables

  if (max_segment_num_to_process==-1)
    max_segment_num_to_process = 
      template_proj_data_info_ptr->get_max_segment_num();
  else
    {
      max_segment_num_to_process =
	min(max_segment_num_to_process, 
	    template_proj_data_info_ptr->get_max_segment_num());
      template_proj_data_info_ptr->
	reduce_segment_range(-max_segment_num_to_process,
			     max_segment_num_to_process);
    }

  const int num_segments = template_proj_data_info_ptr->get_num_segments();
  if (num_segments_in_memory == -1 || interactive)
    num_segments_in_memory = num_segments;
  else
    num_segments_in_memory =
      min(num_segments_in_memory, num_segments);

  // set up normalisation object

    scanner_ptr = 
    new Scanner(*template_proj_data_info_ptr->get_scanner_ptr());

  // TODO this won't work for the HiDAC or so
  proj_data_info_cyl_uncompressed_ptr =
    dynamic_cast<ProjDataInfoCylindricalNoArcCorr *>(
    ProjDataInfo::ProjDataInfoCTI(scanner_ptr, 
                  1, scanner_ptr->get_num_rings()-1,
                  scanner_ptr->get_num_detectors_per_ring()/2,
                  scanner_ptr->get_default_num_arccorrected_bins(), 
                  false));

  if (is_null_ptr(normalisation_ptr))
    {
      //normalisation_ptr = new TrivialBinNormalisation;
      warning("Invalid normalisation object\n");
      return true;
    }

  if (do_pre_normalisation)
    {
      if ( normalisation_ptr->set_up(proj_data_info_cyl_uncompressed_ptr)
	   != Succeeded::yes)
	error("correct_projdata: set-up of normalisation failed\n");
    }
  else
    {
      if ( post_normalisation_ptr->set_up(template_proj_data_info_ptr)
	   != Succeeded::yes)
	error("correct_projdata: set-up of normalisation failed\n");
    }

  // handle time frame definitions etc

  do_time_frame = num_events_to_store<=0;

  if (do_time_frame && frame_definition_filename.size()==0)
    {
      warning("Have to specify either 'frame_definition_filename' or 'num_events_to_store'\n");
      return true;
    }

  if (frame_definition_filename.size()!=0)
    frame_defs = TimeFrameDefinitions(frame_definition_filename);
  else
    {
      // make a single frame starting from 0. End value will be ignored.
      vector<pair<double, double> > frame_times(1, pair<double,double>(0,1));
      frame_defs = TimeFrameDefinitions(frame_times);
    }
  return false;
}

LmToProjData::
LmToProjData()
{}

LmToProjData::
LmToProjData(const char * const par_filename)
{
  set_defaults();
  if (par_filename!=0)
    {
      if (parse(par_filename)==false)
	error("Exiting\n");
    }
  else
    ask_parameters();
}


void
LmToProjData::
get_bin_from_event(Bin& bin, const CListEvent& event) const
{  
  if (do_pre_normalisation)
   {
     Bin uncompressed_bin;
     event.get_bin(uncompressed_bin, *proj_data_info_cyl_uncompressed_ptr);
     if (uncompressed_bin.get_bin_value()<=0)
      return; // rejected for some strange reason


    // do_normalisation
#ifndef FRAME_BASED_DT_CORR
     const double start_time = current_time;
     const double end_time = current_time;
#else
     const double start_time = frame_defs.get_start_time(current_frame_num);
     const double end_time =frame_defs.get_end_time(current_frame_num);
#endif
     
      const float bin_efficiency = 
	normalisation_ptr->get_bin_efficiency(uncompressed_bin,start_time,end_time);
      // TODO remove arbitrary number. Supposes that these bin_efficiencies are around 1
      if (bin_efficiency < 1.E-10)
	{
	warning("\nBin_efficiency %g too low for uncompressed bin (s:%d,v:%d,ax_pos:%d,tang_pos:%d). Event ignored\n",
		bin_efficiency,
		uncompressed_bin.segment_num(), uncompressed_bin.view_num(), 
		uncompressed_bin.axial_pos_num(), uncompressed_bin.tangential_pos_num());
	bin.set_bin_value(-1);
	return;
	}
     
      //	bin.set_bin_value(1/bin_efficiency);
    // do motion correction here

    // now find 'compressed' bin, i.e. taking mashing, span etc into account
    // Also, adjust the normalisation factor according to the number of
    // uncompressed bins in a compressed bin

    const float bin_value = 1/bin_efficiency;
    // TODO wasteful: we decode the event twice. replace by something like
    // template_proj_data_info_ptr->get_bin_from_uncompressed(bin, uncompressed_bin);
    event.get_bin(bin, *template_proj_data_info_ptr);
   
    if (bin.get_bin_value()>0)
      {
	bin.set_bin_value(bin_value);
      }

  }
  else
    {
      event.get_bin(bin, *template_proj_data_info_ptr);
    }

} 

void 
LmToProjData::
do_post_normalisation(Bin& bin) const
{
    if (bin.get_bin_value()>0)
    {
      if (do_pre_normalisation)
	{
	  bin.set_bin_value(bin.get_bin_value()/get_compression_count(bin));
	}
      else
	{
#ifndef FRAME_BASED_DT_CORR
	  const double start_time = current_time;
	  const double end_time = current_time;
#else
	  const double start_time = frame_defs.get_start_time(current_frame_num);
	  const double end_time =frame_defs.get_end_time(current_frame_num);
#endif
	  const float bin_efficiency = post_normalisation_ptr->get_bin_efficiency(bin,start_time,end_time);
	  // TODO remove arbitrary number. Supposes that these bin_efficiencies are around 1
	  if (bin_efficiency < 1.E-10)
	    {
	      warning("\nBin_efficiency %g too low for bin (s:%d,v:%d,ax_pos:%d,tang_pos:%d). Event ignored\n",
		      bin_efficiency,
		      bin.segment_num(), bin.view_num(), bin.axial_pos_num(), bin.tangential_pos_num());
	      bin.set_bin_value(-1);
	    }
	  else
	    {
	      bin.set_bin_value(1/bin_efficiency);
	    }	  
	}
    }
}


int
LmToProjData::
get_compression_count(const Bin& bin) const
{
  // TODO this currently works ONLY for cylindrical PET scanners

   const ProjDataInfoCylindrical& proj_data_info =
      dynamic_cast<const ProjDataInfoCylindrical&>(*template_proj_data_info_ptr);

    return proj_data_info.get_num_ring_pairs_for_segment_axial_pos_num(bin.segment_num(),bin.axial_pos_num())*
			   proj_data_info.get_view_mashing_factor();

}

void
LmToProjData::
process_new_time_event(const CListTime&)
{}


void
LmToProjData::
start_new_time_frame(const unsigned int)
{}

void
LmToProjData::
process_data()
{ 
  CPUTimer timer;
  timer.start();
  
  double time_of_last_stored_event = 0;
  long num_stored_events = 0;
  VectorWithOffset<segment_type *> 
    segments (template_proj_data_info_ptr->get_min_segment_num(), 
	      template_proj_data_info_ptr->get_max_segment_num());
  
  /* Here starts the main loop which will store the listmode data. */
 VectorWithOffset<CListModeData::SavedPosition> 
   frame_start_positions(1, frame_defs.get_num_frames());

 for (current_frame_num = 1;
      current_frame_num<=frame_defs.get_num_frames();
      ++current_frame_num)
   {
     long num_events_in_frame = 0;

     // TODO could be improved by first skipping events not in the frame
     frame_start_positions[current_frame_num] = 
       lm_data_ptr->save_get_position();

     const double start_time = frame_defs.get_start_time(current_frame_num);
     const double end_time = frame_defs.get_end_time(current_frame_num);

     start_new_time_frame(current_frame_num);

     //*********** open output file
       shared_ptr<iostream> output;
       shared_ptr<ProjData> proj_data_ptr;

       {
	 char rest[50];
	 sprintf(rest, "_f%dg1b0d0", current_frame_num);
	 const string output_filename = output_filename_prefix + rest;
      
	 proj_data_ptr = 
	   construct_proj_data(output, output_filename, template_proj_data_info_ptr);
       }
       /*
	 For each start_segment_index, we check which events occur in the
	 segments between start_segment_index and 
	 start_segment_index+num_segments_in_memory.
       */
       for (int start_segment_index = proj_data_ptr->get_min_segment_num(); 
	    start_segment_index <= proj_data_ptr->get_max_segment_num(); 
	    start_segment_index += num_segments_in_memory) 
	 {
	 
	   cerr << "Processing next batch of segments" <<endl;
	   const int end_segment_index = 
	     min( proj_data_ptr->get_max_segment_num()+1, start_segment_index + num_segments_in_memory) - 1;
    
	   if (!interactive)
	     allocate_segments(segments, start_segment_index, end_segment_index, proj_data_ptr->get_proj_data_info_ptr());

	   // the next variable is used to see if there are more events to store for the current segments
	   // num_events_to_store-more_events will be the number of allowed coincidence events currently seen in the file
	   // ('allowed' independent on the fact of we have its segment in memory or not)
	   // When do_time_frame=true, the number of events is irrelevant, so we 
	   // just set more_events to 1, and never change it
	   long more_events = 
	     do_time_frame? 1 : num_events_to_store;

	   // go to the beginning of the listmode data for this frame
	   lm_data_ptr->set_get_position(frame_start_positions[current_frame_num]);
	   {      
	     // loop over all events in the listmode file
	     shared_ptr <CListRecord> record_sptr = lm_data_ptr->get_empty_record_sptr();
	     CListRecord& record = *record_sptr;

	     current_time = start_time;
	     while (more_events)
	       {
		 if (lm_data_ptr->get_next_record(record) == Succeeded::no) 
		   {
		     // no more events in file for some reason
		     break; //get out of while loop
		   }
		 if (record.is_time())
		   {
		     const double new_time = record.time().get_time_in_secs();
		     if (do_time_frame && new_time >= end_time)
		       break; // get out of while loop
		     current_time = new_time;
		     if (current_time>=start_time)
		       process_new_time_event(record.time());
		   }
		 else if (record.is_event() && start_time <= current_time)
		   {
		     Bin bin;
		     // set value in case the event decoder doesn't touch it
		     // otherwise it would be 0 and all events will be ignored
		     bin.set_bin_value(1);
                     get_bin_from_event(bin, record.event());
		     		       
		     // check if it's inside the range we want to store
		     if (bin.get_bin_value()>0
			 && bin.tangential_pos_num()>= proj_data_ptr->get_min_tangential_pos_num()
			 && bin.tangential_pos_num()<= proj_data_ptr->get_max_tangential_pos_num()
			 && bin.axial_pos_num()>=proj_data_ptr->get_min_axial_pos_num(bin.segment_num())
			 && bin.axial_pos_num()<=proj_data_ptr->get_max_axial_pos_num(bin.segment_num())
			 ) 
		       {
			 do_post_normalisation(bin);

			 assert(bin.view_num()>=proj_data_ptr->get_min_view_num());
			 assert(bin.view_num()<=proj_data_ptr->get_max_view_num());
            
			 // see if we increment or decrement the value in the sinogram
			 const int event_increment =
			   record.event().is_prompt() 
			   ? ( store_prompts ? 1 : 0 ) // it's a prompt
			   :  delayed_increment;//it is a delayed-coincidence event
            
			 if (event_increment==0)
			   continue;
            
			 if (!do_time_frame)
			   more_events-= event_increment;
            
			 // now check if we have its segment in memory
			 if (bin.segment_num() >= start_segment_index && bin.segment_num()<=end_segment_index)
			   {
			     num_events_in_frame += event_increment;               
			     num_stored_events += event_increment;
			     if (num_stored_events%500000L==0) cout << "\r" << num_stored_events << flush;
                            
			     if (interactive)
			       printf("Seg %4d view %4d ax_pos %4d tang_pos %4d time %8g stored\n", 
				      bin.segment_num(), bin.view_num(), bin.axial_pos_num(), bin.tangential_pos_num(),
				      current_time);
			     else
			       (*segments[bin.segment_num()])[bin.view_num()][bin.axial_pos_num()][bin.tangential_pos_num()] += 
#ifdef INCLUDE_NORMALISATION_FACTORS
			       bin.get_bin_value() * 
#endif
			       event_increment;
			   }
		       }
		     else 	// event is rejected for some reason
		       {
			 if (interactive)
			   printf("Seg %4d view %4d ax_pos %4d tang_pos %4d time %8g ignored\n", 
				  bin.segment_num(), bin.view_num(), bin.axial_pos_num(), bin.tangential_pos_num(), current_time);
		       }     
		   } // end of spatial event processing
	       } // end of while loop over all events

	     time_of_last_stored_event = 
	       max(time_of_last_stored_event,current_time); 
	   } 

	   if (!interactive)
	   save_and_delete_segments(output, segments, 
				    start_segment_index, end_segment_index, 
				    *proj_data_ptr);  
	 } // end of for loop for segment range
       cerr <<  "\nTotal number of counts stored in this time period: " << num_events_in_frame << endl;
   } // end of loop over frames

 timer.stop();

 cerr << "Last stored event was recorded after time-tick at " << time_of_last_stored_event << " secs\n";
 if (!do_time_frame && 
     (num_stored_events<=0 ||
      /*static_cast<unsigned long>*/(num_stored_events)<num_events_to_store))
   cerr << "Early stop due to EOF. " << endl;
 cerr << "Total number of prompts/trues/delayeds stored: " << num_stored_events << endl;

 cerr << "\nThis took " << timer.value() << "s CPU time." << endl;

}



/************************* Local helper routines *************************/


void 
allocate_segments( VectorWithOffset<segment_type *>& segments,
		  const int start_segment_index, 
		  const int end_segment_index,
		  const ProjDataInfo* proj_data_info_ptr)
{
  
  for (int seg=start_segment_index ; seg<=end_segment_index; seg++)
  {
#ifdef USE_SegmentByView
    segments[seg] = new SegmentByView<elem_type>(
    	proj_data_info_ptr->get_empty_segment_by_view (seg)); 
#else
    segments[seg] = 
      new Array<3,elem_type>(IndexRange3D(0, proj_data_info_ptr->get_num_views()-1, 
				      0, proj_data_info_ptr->get_num_axial_poss(seg)-1,
				      -(proj_data_info_ptr->get_num_tangential_poss()/2), 
				      proj_data_info_ptr->get_num_tangential_poss()-(proj_data_info_ptr->get_num_tangential_poss()/2)-1));
#endif
  }
}

void 
save_and_delete_segments(shared_ptr<iostream>& output,
			 VectorWithOffset<segment_type *>& segments,
			 const int start_segment_index, 
			 const int end_segment_index, 
			 ProjData& proj_data)
{
  
  for (int seg=start_segment_index; seg<=end_segment_index; seg++)
  {
    {
#ifdef USE_SegmentByView
      proj_data.set_segment(*segments[seg]);
#else
      (*segments[seg]).write_data(*output);
#endif
      delete segments[seg];      
    }
    
  }
}



static
shared_ptr<ProjData>
construct_proj_data(shared_ptr<iostream>& output,
                    const string& output_filename, 
                    const shared_ptr<ProjDataInfo>& proj_data_info_ptr)
{
  vector<int> segment_sequence_in_stream(proj_data_info_ptr->get_num_segments());
  { 
#ifndef STIR_NO_NAMESPACES
    std:: // explcitly needed by VC
#endif
    vector<int>::iterator current_segment_iter =
      segment_sequence_in_stream.begin();
    for (int segment_num=proj_data_info_ptr->get_min_segment_num();
         segment_num<=proj_data_info_ptr->get_max_segment_num();
         ++segment_num)
      *current_segment_iter++ = segment_num;
  }
#ifdef USE_SegmentByView
  // don't need output stream in this case
  return new ProjDataInterfile(proj_data_info_ptr, output_filename, ios::out, 
                               segment_sequence_in_stream,
                               ProjDataFromStream::Segment_View_AxialPos_TangPos,
		               OUTPUTNumericType);
#else
  // this code would work for USE_SegmentByView as well, but the above is far simpler...
  output = new fstream (output_filename.c_str(), ios::out|ios::binary);
  if (!*output)
    error("Error opening output file %s\n",output_filename.c_str());
  shared_ptr<ProjDataFromStream> proj_data_ptr = 
    new ProjDataFromStream(proj_data_info_ptr, output, 
                           /*offset=*/0, 
                           segment_sequence_in_stream,
                           ProjDataFromStream::Segment_View_AxialPos_TangPos,
		           OUTPUTNumericType);
  write_basic_interfile_PDFS_header(output_filename, *proj_data_ptr);
  return proj_data_ptr;  
#endif
}


END_NAMESPACE_STIR
