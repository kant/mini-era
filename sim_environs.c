/*
 * Copyright 2019 IBM
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>

#include "sim_environs.h"

/* These are structures, etc. used in the environment */

// This is the master list of all currently live objects in the world.
//  This is a sorted list (by distance) of objects per lane
unsigned global_object_id = 0;
object_state_t* the_objects[5];

// This represents my car.
object_state_t my_car;		



void
print_object(object_state_t* st) {
  printf(" Object # %u ", st->obj_id);
  switch(st->object) {
  case myself: printf("My_Car "); break;
  case no_label : printf("No_Label "); break;
  case car : printf("Car "); break;
  case truck : printf("Truck "); break;
  case pedestrian : printf("Person "); break;
  case bicycle : printf("Bike "); break;
  default: printf("ERROR "); 
  }
  printf("size %.1f in ", st->size);
  switch(st->lane) {
  case lhazard : printf(" L-Hazard "); break;
  case left : printf(" Left "); break;
  case center : printf(" Middle "); break;
  case right : printf(" Right "); break;
  case rhazard : printf(" R-Hazard "); break;
  default: printf("ERROR "); 
  }  
  printf("at distance %.1f speed %u\n", st->distance, st->speed);
}  


void
init_sim_environs()
{
  for (int i = 0; i < 5; i++) {
    the_objects[i] = NULL;
  }

  // Set up the default initial state of my car: Middle lane at medium speed.
  my_car.lane = center;
  my_car.object = myself;
  my_car.speed = 50;
  my_car.previous = NULL;	// not used for my_car
  my_car.next = NULL;		// not used for my_car
}


/* NOTES:
 * In this implementation we do a back-to-front pass for each lane...
 * This ensures that there is "open space" behind a car before it "drops back" into that space
 * BUT this only works (well) if the cars/objects all move at the same rate (as one another).  
 * IF we allow objects to have different speeds, then there are different relative motions, and 
 *  the objects must ALSO avoid one another (presumably by altering their speed)
 */

void
iterate_sim_environs() 
{
  // For each lane in the world
  for (int x = 0; x < 5; x++) {
    // Iterate through the objects in the lane from farthest to closest
    // If this obstacle would move "past" (or "through") another obstacle,
    //   adjust that obstacle's speed (downward) and check it will not collide
    object_state_t* obj = the_objects[x];
    float behind = MAX_DISTANCE;
    while (obj != NULL) {
      int delta_dist = my_car.speed - obj->speed;
      float new_dist  = (obj->distance - (1.0 * delta_dist));
      DEBUG(printf(" Lane %u ddist %d to new_dist %.1f for", x, delta_dist, new_dist); print_object(obj)); 
      if (new_dist < 0) { // Object is "off the world" (we've passed it completely)
	DEBUG(printf("  new_dist < 0 --> drop object from environment\n");
	      printf("   OBJ :"); print_object(obj);
	      printf("   LIST:"); print_object(the_objects[x]);
	      if (obj->next != NULL) { printf("   NEXT:"); print_object(obj->next); }
	      if (obj->previous != NULL) { printf("   PREVIOUS:"); print_object(obj->previous); }
	      );
	// Delete the object from the universe!
	if (obj->previous != NULL) {
	    obj->previous->next = obj->next;
	}
	if (obj->next != NULL) {
	  obj->next->previous = obj->previous;
	}
	if (the_objects[x] == obj) {
	  the_objects[x] = obj->next;
	}
      } else {
	if (new_dist > behind) { //obj->previous->distance >= obj->distance) {
	  // We would collide with the car behind us -- slow down
	  unsigned slower = (int)(new_dist - behind + 0.999);
	  obj->distance = behind;
	  obj->speed -= slower;
	  DEBUG(printf("  new_dist %.1f > %.1f behind value : speed drops to %u\n", new_dist, behind, obj->speed));
	} else {
	  obj->distance = new_dist;
	}
	behind = obj->distance - MIN_OBJECT_DIST;
	DEBUG(printf("  RESULT: Lane %u OBJ", x); print_object(obj));
      }
      obj = obj->next; // move to the next object
    }
  }
  
  // Now determine for each major lane (i.e. Left, Middle, Right) 
  //   whether to add a new object or not...
  for (int x = 1; x < 4; x++) {
    object_state_t * pobj = the_objects[x];
    if ((pobj == NULL) || (pobj->distance < (MAX_DISTANCE - MAX_OBJECT_SIZE - MIN_OBJECT_DIST))) {
      // There is space for a new object to enter
      int num = (rand() % (100)); // Return a value from [0,99]
      if (num > 90) {
        // Create a new object (car) and add it to the lane at position [x][0]
        object_state_t* no_p = (object_state_t*)calloc(1, sizeof(object_state_t));
	no_p->obj_id = global_object_id++;
	no_p->lane = x;
	//no_p->object = car; break;
	int objn = (rand() % 4); // Return a value from [0,99]
	switch(objn) { 
	case 0: no_p->object = car;        no_p->speed = 40;  no_p->size =  5.0; break;
	case 1: no_p->object = truck;      no_p->speed = 30;  no_p->size = 10.0; break;
	case 2: no_p->object = pedestrian; no_p->speed = 10;  no_p->size =  2.0; break;
	case 3: no_p->object = bicycle;    no_p->speed = 20;  no_p->size =  5.0; break;
	}
	no_p->distance = MAX_DISTANCE;
	no_p->previous = NULL;
	no_p->next = the_objects[x];
	if (the_objects[x] != NULL) {
	  the_objects[x]->previous = no_p;
	}
	the_objects[x] = no_p;
	printf("Adding"); print_object(no_p);
      }
    }
  }

  visualize_world();

  // Now we have the state for this (new) time step
  //  Use this to determine my_car's input data, e.g. 
  //  safe_to_move_L/R, etc.
  // NOTE: Currently I am ignoring moving INTO the hazard lanes...
  /**
  message_t viterbi_in_state = -1;
  switch (my_car.lane) {
  case lhazard : 
    if (the_world[1][10] == NULL) { 
      viterbi_in_state = safe_to_move_right_only; 
    } else { 
      viterbi_in_state = unsafe_to_move_left_or_right;
    }
    break;
  case left : 
    if (the_world[2][10] == NULL) { 
      viterbi_in_state = safe_to_move_right_only; 
    } else { 
      viterbi_in_state = unsafe_to_move_left_or_right;
    }
    break;
  case center : 
    if (the_world[3][10] == NULL) { 
      if (the_world[1][10] == NULL) { 
	viterbi_in_state = safe_to_move_right_or_left; 
      } else {
	viterbi_in_state = safe_to_move_right_only;
      }
    } else if (the_world[1][10] == NULL) { 
      viterbi_in_state = safe_to_move_left_only;      
    } else { 
      viterbi_in_state = unsafe_to_move_left_or_right;
    }
    break;
  case right : 
    if (the_world[2][10] == NULL) { 
      viterbi_in_state = safe_to_move_left_only; 
    } else { 
      viterbi_in_state = unsafe_to_move_left_or_right;
    }
    break;
  case rhazard : 
    if (the_world[3][10] == NULL) { 
      viterbi_in_state = safe_to_move_left_only; 
    } else { 
      viterbi_in_state = unsafe_to_move_left_or_right;
    }
    break;
  default: printf("ERROR "); 
  }
  printf(" viterbi_in_state = %d\n", viterbi_in_state);
  do_viterbi_work(viterbi_in_state, false); // set "true" for debug output
  **/
}


void
visualize_world()
{
  // For each lane 
  for (int x = 0; x < 5; x++) {
    // List the objects in the lane
    object_state_t* pobj = the_objects[x];
    if (pobj != NULL) {
      printf("Lane %u has :", x);
      while (pobj != NULL) {
	print_object(pobj);
	pobj = pobj->next;
      }
    } else {
      printf("Lane %u is empty\n", x);
    }
  }
  printf("\n\n");
}



