/*
    This file is part of the example codes which have been used
    for the "Code Optmization Workshop".
    
    Copyright (C) 2016  Fabio Baruffa <fbaru-dev@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <CL/sycl.hpp>
#include <random>

#include <iomanip>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdlib.h>


#include "GSimulation.hpp"
#include "cpu_time.hpp"

using namespace cl::sycl;

GSimulation :: GSimulation()
{
  std::cout << "===============================" << std::endl;
  std::cout << " Initialize Gravity Simulation" << std::endl;
  set_npart(2000); 
  set_nsteps(500);
  set_tstep(0.1); 
  set_sfreq(50);
}

void GSimulation :: set_number_of_particles(int N)  
{
  set_npart(N);
}

void GSimulation :: set_number_of_steps(int N)  
{
  set_nsteps(N);
}

void GSimulation :: init_pos()
{
  std::random_device rd;        //random number generator
  std::mt19937 gen(42);
  std::uniform_real_distribution<real_type> unif_d(0,1.0);

  for(int i=0; i<get_npart(); ++i)
  {
    particles->pos_x[i] = unif_d(gen);
    particles->pos_y[i] = unif_d(gen);
    particles->pos_z[i] = unif_d(gen);
  }
}

void GSimulation :: init_vel()
{
  std::random_device rd;        //random number generator
  std::mt19937 gen(42);
  std::uniform_real_distribution<real_type> unif_d(-1.0,1.0);

  for(int i=0; i<get_npart(); ++i)
  {
    particles->vel_x[i] = unif_d(gen) * 1.0e-3f;
    particles->vel_y[i] = unif_d(gen) * 1.0e-3f;
    particles->vel_z[i] = unif_d(gen) * 1.0e-3f;
  }
}

void GSimulation :: init_acc()
{
  for(int i=0; i<get_npart(); ++i)
  {
    particles->acc_x[i] = 0.f;
    particles->acc_y[i] = 0.f;
    particles->acc_z[i] = 0.f;
  }
}

void GSimulation :: init_mass()
{
  real_type n   = static_cast<real_type> (get_npart());
  std::random_device rd;        //random number generator
  std::mt19937 gen(42);
  std::uniform_real_distribution<real_type> unif_d(0.0,1.0);

  for(int i=0; i<get_npart(); ++i)
  {
    particles->mass[i] = n * unif_d(gen);
  }
}

void GSimulation :: start() 
{
  std::cout << "device option: " << get_devices() << std::endl;
  std::vector<queue> q;
  if (get_devices() == 1) {
    q.push_back(queue(cpu_selector()));
  } else if (get_devices() == 2) {
    q.push_back(queue(gpu_selector()));
  } else {
    q.push_back(queue(cpu_selector()));
    q.push_back(queue(gpu_selector()));
  }

  for (int i = 0; i < q.size(); i++) {
    std::cout << "Device #" << i << ": ";
    std::cout << q[i].get_device().get_info<info::device::name>() << std::endl;
  }


  real_type energy;
  real_type dt = get_tstep();
  int n = get_npart();
  int i;

  const int alignment = 32;
  particles = (ParticleSoA*) _mm_malloc(sizeof(ParticleSoA),alignment);

  particles->pos_x = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->pos_y = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->pos_z = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->vel_x = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->vel_y = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->vel_z = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->acc_x = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->acc_y = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->acc_z = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);
  particles->mass  = (real_type*) _mm_malloc(n*sizeof(real_type),alignment);


//  buffer<ParticleSoA> particles_d(particles);
 
  init_pos();	
  init_vel();
  init_acc();
  init_mass();
  
  print_header();
  
  _totTime = 0.; 
 
  const float softeningSquared = 1.e-3f;
  const float G = 6.67259e-11f;
  
  CPUTime time;
  double ts0 = 0;
  double ts1 = 0;
  double nd = double(n);
  double gflops = 1e-9 * ( (11. + 18. ) * nd*nd  +  nd * 19. );
  double av=0.0, dev=0.0;
  int nf = 0;
  
  const double t0 = time.start();
  for (int s=1; s<=get_nsteps(); ++s)
  {   

   ts0 += time.start(); 
   {
     auto particles_pos_x_d = buffer<real_type, 1>(particles->pos_x, range<1>(n));
     auto particles_pos_y_d = buffer<real_type, 1>(particles->pos_y, range<1>(n));
     auto particles_pos_z_d = buffer<real_type, 1>(particles->pos_z, range<1>(n));

     auto particles_vel_x_d = buffer<real_type, 1>(particles->vel_x, range<1>(n));
     auto particles_vel_y_d = buffer<real_type, 1>(particles->vel_y, range<1>(n));
     auto particles_vel_z_d = buffer<real_type, 1>(particles->vel_z, range<1>(n));

     auto particles_acc_x_d = buffer<real_type, 1>(particles->acc_x, range<1>(n));
     auto particles_acc_y_d = buffer<real_type, 1>(particles->acc_y, range<1>(n));
     auto particles_acc_z_d = buffer<real_type, 1>(particles->acc_z, range<1>(n));

     auto particles_mass_d  = buffer<real_type, 1>(particles->mass, range<1>(n));

     auto particles_pos_x_d1 = buffer<real_type, 1>(particles_pos_x_d,id<1>(0), range<1>(1000));
     auto particles_pos_y_d1 = buffer<real_type, 1>(particles_pos_y_d,id<1>(0), range<1>(1000));
     auto particles_pos_z_d1 = buffer<real_type, 1>(particles_pos_z_d,id<1>(0), range<1>(1000));
     auto particles_pos_x_d2 = buffer<real_type, 1>(particles_pos_x_d,id<1>(1000), range<1>(1000));
     auto particles_pos_y_d2 = buffer<real_type, 1>(particles_pos_y_d,id<1>(1000), range<1>(1000));
     auto particles_pos_z_d2 = buffer<real_type, 1>(particles_pos_z_d,id<1>(1000), range<1>(1000));


    q[0].submit([&] (handler& cgh)  {
     auto particles_acc_x_d1 = buffer<real_type, 1>(particles_acc_x_d,id<1>(0), range<1>(1000));
     auto particles_acc_y_d1 = buffer<real_type, 1>(particles_acc_y_d,id<1>(0), range<1>(1000));
     auto particles_acc_z_d1 = buffer<real_type, 1>(particles_acc_z_d,id<1>(0), range<1>(1000));

       auto particles_acc_x = particles_acc_x_d1.get_access<access::mode::read_write>(cgh);
       auto particles_acc_y = particles_acc_y_d1.get_access<access::mode::read_write>(cgh);
       auto particles_acc_z = particles_acc_z_d1.get_access<access::mode::read_write>(cgh);

       auto particles_vel_x = particles_vel_x_d.get_access<access::mode::read>(cgh);
       auto particles_vel_y = particles_vel_y_d.get_access<access::mode::read>(cgh);
       auto particles_vel_z = particles_vel_z_d.get_access<access::mode::read>(cgh);

       auto particles_pos_x = particles_pos_x_d.get_access<access::mode::read>(cgh);
       auto particles_pos_y = particles_pos_y_d.get_access<access::mode::read>(cgh);
       auto particles_pos_z = particles_pos_z_d.get_access<access::mode::read>(cgh);
       auto particles_pos_x_off = particles_pos_x_d1.get_access<access::mode::read>(cgh);
       auto particles_pos_y_off = particles_pos_y_d1.get_access<access::mode::read>(cgh);
       auto particles_pos_z_off = particles_pos_z_d1.get_access<access::mode::read>(cgh);


       auto particles_mass = particles_mass_d.get_access<access::mode::read>(cgh);


       cgh.parallel_for<class update_accel>(
         range<1>(1000),  [=](id<1> i) {
         //nd_range<1>(range<1>(1000), range<1>()), [=](nd_item<1> item) {
         //  auto i = item.get_global_id();

     real_type ax_i = particles_acc_x[i];
     real_type ay_i = particles_acc_y[i];
     real_type az_i = particles_acc_z[i];

     for (int j = 0; j < n; j++)
     {
       real_type dx, dy, dz;
	     real_type distanceSqr = 0.0f;
	     real_type distanceInv = 0.0f;
	        
	     dx = particles_pos_x[j] - particles_pos_x_off[i];	//1flop
	     dy = particles_pos_y[j] - particles_pos_y_off[i];	//1flop	
	     dz = particles_pos_z[j] - particles_pos_z_off[i];	//1flop
	
 	     distanceSqr = dx*dx + dy*dy + dz*dz + softeningSquared;	//6flops
 	     distanceInv = 1.0f / sqrt(distanceSqr);			//1div+1sqrt

	     ax_i += dx * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     ay_i += dy * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     az_i += dz * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
     }
     particles_acc_x[i] = ax_i;
     particles_acc_y[i] = ay_i;
     particles_acc_z[i] = az_i;

         }); // end of parallel for scope
       }); // end of command group scope

//update host

   

    q[0].submit([&] (handler& cgh)  {
     auto particles_acc_x_d2 = buffer<real_type, 1>(particles_acc_x_d,id<1>(1000), range<1>(1000));
     auto particles_acc_y_d2 = buffer<real_type, 1>(particles_acc_y_d,id<1>(1000), range<1>(1000));
     auto particles_acc_z_d2 = buffer<real_type, 1>(particles_acc_z_d,id<1>(1000), range<1>(1000));
       auto particles_acc_x = particles_acc_x_d2.get_access<access::mode::read_write>(cgh);
       auto particles_acc_y = particles_acc_y_d2.get_access<access::mode::read_write>(cgh);
       auto particles_acc_z = particles_acc_z_d2.get_access<access::mode::read_write>(cgh);

       auto particles_vel_x = particles_vel_x_d.get_access<access::mode::read>(cgh);
       auto particles_vel_y = particles_vel_y_d.get_access<access::mode::read>(cgh);
       auto particles_vel_z = particles_vel_z_d.get_access<access::mode::read>(cgh);

       auto particles_pos_x = particles_pos_x_d.get_access<access::mode::read>(cgh);
       auto particles_pos_y = particles_pos_y_d.get_access<access::mode::read>(cgh);
       auto particles_pos_z = particles_pos_z_d.get_access<access::mode::read>(cgh);
       auto particles_pos_x_off = particles_pos_x_d2.get_access<access::mode::read>(cgh);
       auto particles_pos_y_off = particles_pos_y_d2.get_access<access::mode::read>(cgh);
       auto particles_pos_z_off = particles_pos_z_d2.get_access<access::mode::read>(cgh);

       auto particles_mass = particles_mass_d.get_access<access::mode::read>(cgh);


       cgh.parallel_for<class update_acce2l>(
         range<1>(1000),  [=](id<1> i) {
//         nd_range<1>(range<1>(1000), range<1>()), [=](nd_item<1> item) {
//           auto i = item.get_global_id();

     real_type ax_i = particles_acc_x[i];
     real_type ay_i = particles_acc_y[i];
     real_type az_i = particles_acc_z[i];

     for (int j = 0; j < n; j++)
     {
       real_type dx, dy, dz;
	     real_type distanceSqr = 0.0f;
	     real_type distanceInv = 0.0f;
	        
	     dx = particles_pos_x[j] - particles_pos_x_off[i];	//1flop
	     dy = particles_pos_y[j] - particles_pos_y_off[i];	//1flop	
	     dz = particles_pos_z[j] - particles_pos_z_off[i];	//1flop
	
 	     distanceSqr = dx*dx + dy*dy + dz*dz + softeningSquared;	//6flops
 	     distanceInv = 1.0f / sqrt(distanceSqr);			//1div+1sqrt

	     ax_i += dx * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     ay_i += dy * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     az_i += dz * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
     }
     particles_acc_x[i] = ax_i;
     particles_acc_y[i] = ay_i;
     particles_acc_z[i] = az_i;

         }); // end of parallel for scope
       }); // end of command group scope

#if 0
    q_cpu.submit([&] (handler& cgh)  {
       auto particles_acc_x = particles_acc_x_d2.get_access<access::mode::read_write>(cgh);
       auto particles_acc_y = particles_acc_y_d2.get_access<access::mode::read_write>(cgh);
       auto particles_acc_z = particles_acc_z_d2.get_access<access::mode::read_write>(cgh);

       auto particles_vel_x = particles_vel_x_d2.get_access<access::mode::read>(cgh);
       auto particles_vel_y = particles_vel_y_d2.get_access<access::mode::read>(cgh);
       auto particles_vel_z = particles_vel_z_d2.get_access<access::mode::read>(cgh);

       auto particles_pos_x = particles_pos_x_d2.get_access<access::mode::read>(cgh);
       auto particles_pos_y = particles_pos_y_d2.get_access<access::mode::read>(cgh);
       auto particles_pos_z = particles_pos_z_d2.get_access<access::mode::read>(cgh);

       auto particles_mass = particles_mass_d2.get_access<access::mode::read>(cgh);


       cgh.parallel_for<class update_accel2>(
         nd_range<1>(range<1>(1000), range<1>()), [=](nd_item<1> item) {
           auto i = item.get_global_id();

     real_type ax_i = particles_acc_x[i];
     real_type ay_i = particles_acc_y[i];
     real_type az_i = particles_acc_z[i];

     for (int j = 0; j < n; j++)
     {
       real_type dx, dy, dz;
	     real_type distanceSqr = 0.0f;
	     real_type distanceInv = 0.0f;
	        
	     dx = particles_pos_x[j] - particles_pos_x[i];	//1flop
	     dy = particles_pos_y[j] - particles_pos_y[i];	//1flop	
	     dz = particles_pos_z[j] - particles_pos_z[i];	//1flop
	
 	     distanceSqr = dx*dx + dy*dy + dz*dz + softeningSquared;	//6flops
 	     distanceInv = 1.0f / sqrt(distanceSqr);			//1div+1sqrt

	     ax_i += dx * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     ay_i += dy * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
	     az_i += dz * G * particles_mass[j] * distanceInv * distanceInv * distanceInv; //6flops
     }
     particles_acc_x[i] = ax_i;
     particles_acc_y[i] = ay_i;
     particles_acc_z[i] = az_i;

         }); // end of parallel for scope
       }); // end of command group scope
#endif
#if 0
    q.submit([&] (handler& cgh)  {
       auto particles_acc_x = particles_acc_x_d1.get_access<access::mode::write>(cgh);
       auto particles_acc_y = particles_acc_y_d1.get_access<access::mode::write>(cgh);
       auto particles_acc_z = particles_acc_z_d1.get_access<access::mode::write>(cgh);

       auto particles_vel_x = particles_vel_x_d1.get_access<access::mode::read_write>(cgh);
       auto particles_vel_y = particles_vel_y_d1.get_access<access::mode::read_write>(cgh);
       auto particles_vel_z = particles_vel_z_d1.get_access<access::mode::read_write>(cgh);

       auto particles_pos_x = particles_pos_x_d1.get_access<access::mode::read_write>(cgh);
       auto particles_pos_y = particles_pos_y_d1.get_access<access::mode::read_write>(cgh);
       auto particles_pos_z = particles_pos_z_d1.get_access<access::mode::read_write>(cgh);


       cgh.parallel_for<class update_energy>(
         range<1>(range<1>(n)), [=](item<1> item) {
           auto i = item.get_id();

     particles_vel_x[i] += particles_acc_x[i] * dt; //2flops
     particles_vel_y[i] += particles_acc_y[i] * dt; //2flops
     particles_vel_z[i] += particles_acc_z[i] * dt; //2flops
	  
     particles_pos_x[i] += particles_vel_x[i] * dt; //2flops
     particles_pos_y[i] += particles_vel_y[i] * dt; //2flops
     particles_pos_z[i] += particles_vel_z[i] * dt; //2flops


         particles_acc_x[i] = 0.;
         particles_acc_y[i] = 0.;
         particles_acc_z[i] = 0.;
         });
       }); 
#endif
   } // end of buffer scope

   // no device side reductions so have to do it here
   energy = 0;
   for (int i = 0; i < n; ++i)// update position
   {

     particles->vel_x[i] += particles->acc_x[i] * dt; //2flops
     particles->vel_y[i] += particles->acc_y[i] * dt; //2flops
     particles->vel_z[i] += particles->acc_z[i] * dt; //2flops
	  
     particles->pos_x[i] += particles->vel_x[i] * dt; //2flops
     particles->pos_y[i] += particles->vel_y[i] * dt; //2flops
     particles->pos_z[i] += particles->vel_z[i] * dt; //2flops


         particles->acc_x[i] = 0.;
         particles->acc_y[i] = 0.;
         particles->acc_z[i] = 0.;

     energy += particles->mass[i] * (
	       particles->vel_x[i]*particles->vel_x[i] + 
               particles->vel_y[i]*particles->vel_y[i] +
               particles->vel_z[i]*particles->vel_z[i]); //7flops
   }
  _kenergy = 0.5 * energy; 

    
    ts1 += time.stop();
    if(!(s%get_sfreq()) ) 
    {
      nf += 1;      
      std::cout << " " 
		<<  std::left << std::setw(8)  << s
		<<  std::left << std::setprecision(5) << std::setw(8)  << s*get_tstep()
		<<  std::left << std::setprecision(5) << std::setw(12) << _kenergy
		<<  std::left << std::setprecision(5) << std::setw(12) << (ts1 - ts0)
		<<  std::left << std::setprecision(5) << std::setw(12) << gflops*get_sfreq()/(ts1 - ts0)
		<<  std::endl;
      if(nf > 2) 
      {
	av  += gflops*get_sfreq()/(ts1 - ts0);
	dev += gflops*get_sfreq()*gflops*get_sfreq()/((ts1-ts0)*(ts1-ts0));
      }
      
      ts0 = 0;
      ts1 = 0;
    }
  
  } //end of the time step loop
  
  const double t1 = time.stop();
  _totTime  = (t1-t0);
  _totFlops = gflops*get_nsteps();
  
  av/=(double)(nf-2);
  dev=sqrt(dev/(double)(nf-2)-av*av);
  
  int nthreads=1;

  std::cout << std::endl;
  std::cout << "# Number Threads     : " << nthreads << std::endl;	   
  std::cout << "# Total Time (s)     : " << _totTime << std::endl;
  std::cout << "# Average Perfomance : " << av << " +- " <<  dev << std::endl;
  std::cout << "===============================" << std::endl;

}


void GSimulation :: print_header()
{
	    
  std::cout << " nPart = " << get_npart()  << "; " 
	    << "nSteps = " << get_nsteps() << "; " 
	    << "dt = "     << get_tstep()  << std::endl;
	    
  std::cout << "------------------------------------------------" << std::endl;
  std::cout << " " 
	    <<  std::left << std::setw(8)  << "s"
	    <<  std::left << std::setw(8)  << "dt"
	    <<  std::left << std::setw(12) << "kenergy"
	    <<  std::left << std::setw(12) << "time (s)"
	    <<  std::left << std::setw(12) << "GFlops"
	    <<  std::endl;
  std::cout << "------------------------------------------------" << std::endl;


}

GSimulation :: ~GSimulation()
{
  _mm_free(particles->pos_x);
  _mm_free(particles->pos_y);
  _mm_free(particles->pos_z);
  _mm_free(particles->vel_x);
  _mm_free(particles->vel_y);
  _mm_free(particles->vel_z);
  _mm_free(particles->acc_x);
  _mm_free(particles->acc_y);
  _mm_free(particles->acc_z);
  _mm_free(particles->mass);
  _mm_free(particles);

}
