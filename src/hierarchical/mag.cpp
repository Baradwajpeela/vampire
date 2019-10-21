//------------------------------------------------------------------------------
//
//   This file is part of the VAMPIRE open source package under the
//   Free BSD licence (see licence file for details).
//
//   (c) Sarah Jenkins, Andrea Meo and Richard F L Evans 2019.
//   All rights reserved.
//
//------------------------------------------------------------------------------
//

// C++ standard library headers
#include <cstdlib>

// Vampire headers
#include "cells.hpp" // needed for dp::cell_id_array but to be removed
#include "vmpi.hpp"

// hierarchical module headers
#include "hierarchical.hpp"
#include "internal.hpp"

// alias internal hierarchical namespace for brevity
namespace ha = hierarchical::internal;

namespace hierarchical{
namespace internal{

//-----------------------------------------------------------------------------
// Function for calculate magnetisation in hierarchical cells
//-----------------------------------------------------------------------------
// As cells are duplicated on all processors, but local cells are not unique,
// the accumulation of moments must be done directly from the atomic positions.
// The total moments are then reduced and broadcast to all processors.
//-----------------------------------------------------------------------------
void calculate_hierarchical_magnetisation(std::vector <double>& x_spin_array, // atomic spin directions
                                          std::vector <double>& y_spin_array,
                                          std::vector <double>& z_spin_array,
                                          std::vector <double>& m_spin_array, // atomic spin moment
                                          std::vector < bool >& magnetic){ // is magnetic

   // initialise local cells to zero (distributed operation)
   for(int cell_index = 0; cell_index < cells::num_local_cells; ++cell_index ) {

      // get cell ID for global array
      const int cell = cells::cell_id_array[cell_index];

      ha::mag_array_x[cell] = 0.0;
      ha::mag_array_y[cell] = 0.0;
      ha::mag_array_z[cell] = 0.0;

   }

   // calculate total moment in each local cell looping over local atoms
   for(int atom = 0; atom < vmpi::num_local_atoms; ++atom) {

      // get cell_ID for atom
      const int cell = cells::atom_cell_id_array[atom];

      // copy spin moment to temporary variable for performance
      const double mus = m_spin_array[atom];

      // Consider only magnetic elements
      if( magnetic[atom] ){
         ha::mag_array_x[cell] += x_spin_array[atom] * mus;
         ha::mag_array_y[cell] += y_spin_array[atom] * mus;
         ha::mag_array_z[cell] += z_spin_array[atom] * mus;
      }

   }

   //--------------------------------------------------------------------------------------
   // loop over all hierarchical levels, computing partial cell magnetizations at level L
   //--------------------------------------------------------------------------------------
   for (int level = 1; level < ha::num_levels; level++ ){

      // determine starting and end indices for level in 1D hierarchical cell array
      int start = ha::cells_level_start_index[level];
      int end   = ha::cells_level_end_index[level];

      // loop over all cells in level L
      for (int cell = start; cell < end; cell++){

         // determine which lower level cells L-1 are in each higher level cell
         int start_cell_in_cell = ha::cells_in_cells_start_index[cell];
         int end_cell_in_cell = ha::cells_in_cells_end_index[cell];

         // initialise higher level L cell magnetization to zero
         ha::mag_array_x[cell] = 0.0;
         ha::mag_array_y[cell] = 0.0;
         ha::mag_array_z[cell] = 0.0;

         // loop over all L-1 level cells and accumulate magnetization
         for (int cell_in_cell = start_cell_in_cell; cell_in_cell < end_cell_in_cell; cell_in_cell++){

            // get local cell ID of subcell
            int subcell = ha::cells_in_cells[cell_in_cell];

            // accumulate cell magnetizations up the hierarchical tree
            ha::mag_array_x[cell] += ha::mag_array_x[subcell];
            ha::mag_array_y[cell] += ha::mag_array_y[subcell];
            ha::mag_array_z[cell] += ha::mag_array_z[subcell];

         }
      }
   }

   // Finally accumulate and sum moments for all cells on all processors
   vmpi::all_reduce_sum(ha::mag_array_x);
   vmpi::all_reduce_sum(ha::mag_array_y);
   vmpi::all_reduce_sum(ha::mag_array_z);

   return;

}

} // end of internal namespace
} // end of hierarchical namespace
